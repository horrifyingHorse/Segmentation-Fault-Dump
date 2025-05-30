#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <ostream>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#define LOG_TICK(ticks) std::cout << ticks;
#define LOG(tick, device, procData) \
  std::cout << tick << "\t" << device << "\t\t" << procData << "\n";
#define LOG_DEBUG(name, label, info) \
  std::cout << name << "\n\t\t" << label "\t" << info;

typedef struct Process {
  enum State { READY, RUNNING, BLOCKED, TERMINATED };
  std::string procName;
  size_t arrivalTime = SIZE_MAX;
  size_t burstTimeCPU = SIZE_MAX;
  size_t burstTimeIO = SIZE_MAX;
  size_t burstTimeRate = SIZE_MAX;  // IO burst after every n CPU bursts
  size_t startTime = SIZE_MAX;
  size_t completionTime;
  size_t burstRemainCPU = SIZE_MAX;
  size_t lastIOBurst = 0;
  long int saveContextOfq = 0;
  State state;

  Process() {}
  Process(const std::string& name,
          size_t at,
          size_t btCPU,
          size_t btIO,
          size_t btr) {
    procName = std::move(name);
    arrivalTime = at;
    burstTimeCPU = btCPU;
    burstRemainCPU = btCPU;
    burstTimeIO = btIO;
    burstTimeRate = btr;
  }
  State exec() {
    state = State::RUNNING;
    if (--burstRemainCPU <= 0) {
      state = State::TERMINATED;
    } else if (++lastIOBurst >= burstTimeRate) {
      refreshIOBurst();
      state = State::BLOCKED;
    }
    return state;
  }
  void refreshIOBurst() { lastIOBurst = 0; }
  size_t turnAroundTime() { return completionTime - arrivalTime; }
  size_t waitingTime() { return turnAroundTime() - burstTimeCPU; }
  size_t responseTime() { return startTime - arrivalTime; }
} Process;
typedef std::vector<Process> Processes;

enum Scheduler { SJF, SRTF, RR, VRR };
typedef std::vector<Scheduler> Schedulers;

class IQueue {
 public:
  virtual ~IQueue() = default;
  virtual void push(const Process& p) = 0;
  virtual void pop() = 0;
  virtual Process top() const = 0;
  virtual bool empty() const = 0;
  virtual void clear() = 0;
};
class RegularQ : public IQueue {
 public:
  void push(const Process& p) override { q.push(p); }
  void pop() override { q.pop(); }
  Process top() const override { return q.front(); }
  bool empty() const override { return q.empty(); }
  void clear() override { q = {}; }

 private:
  std::queue<Process> q = {};
};
class RemainPriorityQ : public IQueue {
 public:
  RemainPriorityQ() : q(cmpRemain) {}
  void push(const Process& p) override { q.push(p); }
  void pop() override { q.pop(); }
  Process top() const override { return q.top(); }
  bool empty() const override { return q.empty(); }
  void clear() override {
    while (!q.empty())
      q.pop();
  }

 private:
  inline static auto cmpRemain = [](const Process& left, const Process& right) {
    if (left.burstRemainCPU != right.burstRemainCPU)
      return left.burstRemainCPU > right.burstRemainCPU;
    return left.arrivalTime > right.arrivalTime;
  };
  std::priority_queue<Process, Processes, decltype(cmpRemain)> q;
};
class BurstPriorityQ : public IQueue {
 public:
  BurstPriorityQ() : q(cmp) {}
  void push(const Process& p) override { q.push(p); }
  void pop() override { q.pop(); }
  Process top() const override { return q.top(); }
  bool empty() const override { return q.empty(); }
  void clear() override {
    while (!q.empty())
      q.pop();
  }

 private:
  inline static auto cmp = [](const Process& left, const Process& right) {
    if (left.burstTimeCPU != right.burstTimeCPU)
      return left.burstTimeCPU > right.burstTimeCPU;
    return left.arrivalTime > right.arrivalTime;
  };
  std::priority_queue<Process, Processes, decltype(cmp)> q;
};

class Device {
 public:
  typedef struct SchedInfo {
    Scheduler sched = Scheduler::SJF;
    long int q = 0;
  } SchedInfo;

  Device() { readyQ = &regularReadyQ; }
  void init(Processes& procs) {
    this->procs = procs;
    totalProc = procs.size();
  }
  void init(Processes& procs, SchedInfo sInfo) {
    this->procs = procs;
    this->totalProc = procs.size();
    this->scheduler = sInfo.sched;
    this->timeQuantum = sInfo.q;

    if (scheduler == Scheduler::SJF) {
      this->readyQ = &burstReadyQ;
    } else if (scheduler == Scheduler::SRTF) {
      this->readyQ = &remainReadyQ;
    } else {
      this->readyQ = &regularReadyQ;
    }
  }

  void start() {
    CleanUp();
    Processor();
  }

  void debug() {
    for (auto& proc : completedProcs) {
      LOG_DEBUG(proc.procName, "Arrival Time:\t", proc.arrivalTime)
      LOG_DEBUG("", "Start Time:\t", proc.startTime)
      LOG_DEBUG("", "Response Time:\t", proc.responseTime())
      LOG_DEBUG("", "Completion Time:", proc.completionTime)
      LOG_DEBUG("", "Turnaround Time:", proc.turnAroundTime())
      LOG_DEBUG("", "Waiting Time:\t", proc.waitingTime() << "\n")
    }
    LOG("Avg Waiting Time", avgWaitingTime(), "");
    LOG("Avg Turnaround Time", avgTurnAroundTime(), "");
    LOG("Avg Response Time", avgResponseTime(), "");
    LOG("Ticks CPU Idle\t", ticksCPUIdle, "");
    LOG("Total Ticks CPU\t", ticksCPU, "");
    LOG("Total CPU Usage\t", usageCPU() << " %", "");
    LOG("CPU Trhoughput\t", throughput(), "");
  }

  double avgWaitingTime() {
    double sum = 0;
    for (auto& proc : completedProcs) {
      sum += proc.waitingTime();
    }
    return (double)(sum / completedProcs.size());
  }

  double avgTurnAroundTime() {
    double sum = 0;
    for (auto& proc : completedProcs) {
      sum += proc.turnAroundTime();
    }
    return (double)(sum / completedProcs.size());
  }

  double avgResponseTime() {
    double sum = 0;
    for (auto& proc : completedProcs) {
      sum += proc.responseTime();
    }
    return (double)(sum / completedProcs.size());
  }

  double usageCPU() {
    return ((double)(ticksCPU - ticksCPUIdle) / ticksCPU) * 100;
  }

  double throughput() { return (double)completedProcs.size() / ticksCPU; }

 private:
  Process execProc = {};
  Processes completedProcs = {};
  Processes procs = {};
  long int timeQuantum = 0;
  long int q = 0;
  size_t totalProc = 0;
  size_t ticksCPU = 0;
  size_t ticksCPUIdle = 0;
  bool isCPUIdle = true;
  Scheduler scheduler = Scheduler::SJF;

  size_t countIOBurst = 0;
  bool isIOIdle = true;
  Process execProcIO;

  IQueue* readyQ;
  BurstPriorityQ burstReadyQ;
  RemainPriorityQ remainReadyQ;
  RegularQ regularReadyQ;
  RegularQ ioQ, auxQ;

  void CleanUp() {
    execProc = {};
    completedProcs = {};
    q = 0;
    ticksCPU = 0;
    ticksCPUIdle = 0;
    isCPUIdle = true;

    countIOBurst = 0;
    isIOIdle = true;
    execProcIO = {};

    ioQ.clear();
    auxQ.clear();
    readyQ->clear();
  }

  void Processor() {
    LOG("Time (tick)", "Device", "Process Served")
    while (totalProc) {
      LOG_TICK(ticksCPU)
      if (isCPUIdle) {
        LOG("\t", "CPU", "-");
      }

      FreshArrivals();
      if (!isCPUIdle) {
        ExecCPU();
      }
      IODevice();
      if (ToSchedule()) {
        ScheduleProc();
      }

      if (isCPUIdle) {
        ticksCPUIdle++;
      }
      ticksCPU++;
      q++;
      LOG("", "", "")
    }
    ticksCPU--;
    ticksCPUIdle--;
  }

  void IODevice() {
    if (!isIOIdle) {
      ExecIO();
    }

    if (isIOIdle && !ioQ.empty()) {
      execProcIO = ioQ.top();
      ioQ.pop();
      countIOBurst = 0;
      isIOIdle = false;
      LOG("\t", "IO", execProcIO.procName << "[Sched]:" << countIOBurst)
    }
  }

  void FreshArrivals() {
    int index = 0;
    for (auto& proc : procs) {
      if (proc.arrivalTime == ticksCPU) {
        LOG("\t", "CPU", proc.procName << "[Arrive]")
        proc.state = Process::State::READY;
        readyQ->push(proc);
        procs.erase(procs.begin() + index);
        continue;
      }
      index++;
    }
  }

  void ExecCPU() {
    execProc.exec();
    if (execProc.state == Process::State::TERMINATED) {
      LOG("\t", "CPU", execProc.procName << "[Comp]");
      isCPUIdle = true;
      totalProc--;
      execProc.completionTime = ticksCPU;
      completedProcs.push_back(execProc);
      execProc = {};
    } else if (execProc.state == Process::State::BLOCKED) {
      LOG("\t", "CPU",
          execProc.procName << "[Q IO]:" << execProc.burstRemainCPU);
      execProc.saveContextOfq = (timeQuantum != 0) ? (q + 1) % timeQuantum : 0;
      ioQ.push(execProc);
      isCPUIdle = true;
      execProc = {};
    } else {
      LOG("\t", "CPU", execProc.procName << ":" << execProc.burstRemainCPU)
    }
  }

  void ScheduleProc() {
    Process proc;
    if (!auxQ.empty()) {
      proc = auxQ.top();
      auxQ.pop();
      q = proc.saveContextOfq - 1;
    } else {
      proc = readyQ->top();
      readyQ->pop();
      q = -1;
    }
    LOG("\t", "CPU", proc.procName << "[Sched]")
    if (!isCPUIdle) {
      readyQ->push(execProc);
    }
    execProc = proc;
    execProc.startTime = std::min(execProc.startTime, ticksCPU);
    isCPUIdle = false;
  }

  bool ToSchedule() {
    switch (scheduler) {
      case Scheduler::SJF:
        return isCPUIdle && !readyQ->empty();
      case Scheduler::SRTF:
        return !readyQ->empty() && (isCPUIdle || readyQ->top().burstRemainCPU <
                                                     execProc.burstRemainCPU);
      case Scheduler::RR:
        return !readyQ->empty() && (isCPUIdle || q + 1 >= timeQuantum);
      case Scheduler::VRR:
        return (!readyQ->empty() || !auxQ.empty()) &&
               (isCPUIdle || q + 1 >= timeQuantum);
    }
    return false;
  }

  void ExecIO() {
    if (++countIOBurst >= execProcIO.burstTimeIO) {
      LOG("\t", "IO", execProcIO.procName << "[Comp]:" << countIOBurst)
      if (scheduler == Scheduler::VRR) {
        auxQ.push(execProcIO);
      } else {
        readyQ->push(execProcIO);
      }
      execProcIO = {};
      isIOIdle = true;
    } else {
      LOG("\t", "IO", execProcIO.procName << ":" << countIOBurst)
    }
  }
};

Processes GetProcs() {
#define GetIndex(exec)                           \
  if (indx != 0) {                               \
    sv.remove_prefix(indx + 1);                  \
  }                                              \
  indx = sv.find_first_of(";");                  \
  if (indx == std::string::npos) {               \
    std::cout << "Invalid Format in procs.proc"; \
    exit(1);                                     \
  }                                              \
  exec

  std::fstream procFile("procs.proc");
  if (!procFile) {
    std::cout << "Unable to open procs.proc";
    exit(1);
  }
  int indx = 0;
  Processes procs;
  std::string line;
  std::string_view sv;
  std::string procName;
  size_t arrivalTime = SIZE_MAX;
  size_t burstTimeCPU = SIZE_MAX;
  size_t burstTimeIO = SIZE_MAX;
  size_t burstTimeRate = SIZE_MAX;
  size_t burstRemainCPU = SIZE_MAX;

  while (std::getline(procFile, line)) {
    indx = 0;
    sv = line;
    GetIndex(procName = sv.substr(0, indx));
    GetIndex(arrivalTime = std::stoull((std::string)sv.substr(0, indx)));
    GetIndex(burstTimeCPU = std::stoull((std::string)sv.substr(0, indx)));
    GetIndex(burstTimeIO = std::stoull((std::string)sv.substr(0, indx)));
    sv.remove_prefix(indx + 1);
    burstTimeRate = std::stoull((std::string)sv);
    procs.push_back(Process(procName, arrivalTime, burstTimeCPU, burstTimeIO,
                            burstTimeRate));
  }
  return std::move(procs);
}

int main(int argc, char* argv[]) {
  Schedulers schedulers;
  std::vector<std::string> args(argv + 1, argv + argc);
  for (const auto& arg : args) {
    if (arg == "sjf") {
      schedulers.push_back(Scheduler::SJF);
    } else if (arg == "srtf") {
      schedulers.push_back(Scheduler::SRTF);
    } else if (arg == "rr") {
      schedulers.push_back(Scheduler::RR);
    } else if (arg == "vrr") {
      schedulers.push_back(Scheduler::VRR);
    } else {
      std::cout << "Invalid Argument: " << arg;
      return 1;
    }
  }
  if (schedulers.size() == 0) {
    schedulers.push_back(Scheduler::SJF);
  }

  Processes procs = GetProcs();
  Device d;
  for (const auto& scheduler : schedulers) {
    d.init(procs, {.sched = scheduler, .q = 5});
    d.start();
    d.debug();
  }

  return 0;
}
