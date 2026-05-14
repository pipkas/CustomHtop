type StateFilter = "all" | "sleeping" | "stopped" | "zombie" | "running" | "idle";
type SortDirection = "asc" | "desc";

interface MemoryInfo {
  total: number;
  used: number;
  free: number;
  available: number;
  buffers: number;
  cache: number;
}

interface SwapInfo {
  total: number;
  used: number;
  free: number;
}

interface CpuInfo {
  id: number;
  total: number;
  low: number;
  high: number;
  kernel: number;
  background: number;
}

interface ProcessInfo {
  pid: number;
  tid: number;
  processPid: number;
  signalPid: number;
  isThread: boolean;
  ppid: number;
  pgrp: number;
  session: number;
  ttyNr: number;
  user: string;
  priority: number;
  nice: number;
  threads: number;
  state: string;
  stateName: string;
  cpu: number;
  mem: number;
  virt: number;
  res: number;
  shr: number;
  time: string;
  comm: string;
  command: string;
  depth?: number;
  treePrefix?: string;
  hasChildren?: boolean;
  childCount?: number;
  rowMatches?: boolean;
}

interface Snapshot {
  timestamp: number;
  memory: MemoryInfo;
  swap: SwapInfo;
  states: Record<string, number>;
  cpus: CpuInfo[];
  processes: ProcessInfo[];
}

interface LinuxSignal {
  number: number;
  name: string;
  description: string;
}

interface SignalResponse {
  ok: boolean;
  pid?: number;
  signal?: number;
  signalName?: string;
  errno?: number;
  error?: string;
}

interface Column {
  key: keyof ProcessInfo;
  label: string;
  numeric?: boolean;
  format?: (process: ProcessInfo) => string;
}

const columns: Column[] = [
  { key: "pid", label: "PID", numeric: true },
  { key: "user", label: "USER" },
  { key: "priority", label: "PRI", numeric: true },
  { key: "virt", label: "VIRT", numeric: true, format: (p) => humanBytes(p.virt) },
  { key: "res", label: "RES", numeric: true, format: (p) => humanBytes(p.res) },
  { key: "shr", label: "SHR", numeric: true, format: (p) => humanBytes(p.shr) },
  { key: "state", label: "S" },
  { key: "cpu", label: "CPU%", numeric: true, format: (p) => p.cpu.toFixed(1) },
  { key: "mem", label: "MEM%", numeric: true, format: (p) => p.mem.toFixed(1) },
  { key: "time", label: "TIME+" },
  { key: "ppid", label: "PPID", numeric: true },
  { key: "pgrp", label: "PGRP", numeric: true },
  { key: "session", label: "SESSION", numeric: true },
  { key: "threads", label: "THR", numeric: true },
  { key: "command", label: "Command" },
];

const state = {
  snapshot: null as Snapshot | null,
  signals: [] as LinuxSignal[],
  sortKey: "cpu" as keyof ProcessInfo,
  sortDirection: "desc" as SortDirection,
  filter: "all" as StateFilter,
  search: "",
  paused: false,
  refreshMs: 1000,
  selectedPid: null as number | null,
  tree: false,
  showThreads: false,
  highlightNew: true,
  processFocus: false,
  collapsedPids: new Set<number>(),
  previousPids: new Set<number>(),
  newUntil: new Map<number, number>(),
  signalTargetPid: null as number | null,
  timer: 0,
};

const els = {
  statusLine: byId("statusLine"),
  themeToggle: byId<HTMLButtonElement>("themeToggle"),
  pauseToggle: byId<HTMLButtonElement>("pauseToggle"),
  refreshInput: byId<HTMLInputElement>("refreshInput"),
  memoryText: byId("memoryText"),
  swapText: byId("swapText"),
  memoryUsed: byId("memoryUsed"),
  memoryBuffers: byId("memoryBuffers"),
  memoryCache: byId("memoryCache"),
  swapUsed: byId("swapUsed"),
  cpuCount: byId("cpuCount"),
  cpuGrid: byId("cpuGrid"),
  sortSelect: byId<HTMLSelectElement>("sortSelect"),
  sortDirection: byId<HTMLButtonElement>("sortDirection"),
  stateFilters: byId("stateFilters"),
  treeToggle: byId<HTMLInputElement>("treeToggle"),
  threadsToggle: byId<HTMLInputElement>("threadsToggle"),
  highlightToggle: byId<HTMLInputElement>("highlightToggle"),
  processFocusToggle: byId<HTMLButtonElement>("processFocusToggle"),
  searchInput: byId<HTMLInputElement>("searchInput"),
  tableHead: byId("tableHead"),
  processRows: byId("processRows"),
  contextMenu: byId<HTMLElement>("contextMenu"),
  signalDialog: byId<HTMLDialogElement>("signalDialog"),
  signalTitle: byId("signalTitle"),
  signalSearch: byId<HTMLInputElement>("signalSearch"),
  signalList: byId("signalList"),
  toastStack: byId("toastStack"),
};

function byId<T extends HTMLElement = HTMLElement>(id: string): T {
  const element = document.getElementById(id);
  if (!element) {
    throw new Error(`Missing element #${id}`);
  }
  return element as T;
}

function humanBytes(value: number): string {
  if (!Number.isFinite(value) || value <= 0) {
    return "0";
  }
  const units = ["B", "K", "M", "G", "T"];
  let current = value;
  let unit = 0;
  while (current >= 1024 && unit < units.length - 1) {
    current /= 1024;
    unit += 1;
  }
  return `${current >= 10 || unit === 0 ? current.toFixed(0) : current.toFixed(1)}${units[unit]}`;
}

function percent(part: number, total: number): number {
  if (!total || total <= 0) {
    return 0;
  }
  return Math.max(0, Math.min(100, (part * 100) / total));
}

function setJar(memory: MemoryInfo, swap: SwapInfo): void {
  const used = percent(memory.used, memory.total);
  const buffers = percent(memory.buffers, memory.total);
  const cache = percent(memory.cache, memory.total);

  els.memoryUsed.style.height = `${used}%`;
  els.memoryBuffers.style.height = `${buffers}%`;
  els.memoryBuffers.style.bottom = `${used}%`;
  els.memoryCache.style.height = `${cache}%`;
  els.memoryCache.style.bottom = `${used + buffers}%`;
  els.memoryText.textContent = `${humanBytes(memory.used)} / ${humanBytes(memory.total)}`;

  const swapUsed = percent(swap.used, swap.total);
  els.swapUsed.style.height = `${swapUsed}%`;
  els.swapUsed.classList.toggle("critical", swapUsed >= 80);
  els.swapText.textContent = swap.total > 0
    ? `${humanBytes(swap.used)} / ${humanBytes(swap.total)}`
    : "нет swap";
}

function renderCpus(cpus: CpuInfo[]): void {
  els.cpuCount.textContent = `${cpus.length} CPU${cpus.length !== 1 ? "s" : ""}`;
  els.cpuGrid.replaceChildren(...cpus.map((cpu) => {
    const row = document.createElement("div");
    row.className = "cpu-row";

    const name = document.createElement("span");
    name.textContent = `CPU ${cpu.id}`;

    const bar = document.createElement("div");
    bar.className = "cpu-bar";
    let left = 0;
    for (const [className, rawWidth] of [
      ["low", cpu.low],
      ["high", cpu.high],
      ["kernel", cpu.kernel],
      ["background", cpu.background],
    ] as const) {
      const segment = document.createElement("span");
      const width = Math.max(0, Math.min(100 - left, rawWidth));
      segment.className = `cpu-seg ${className}`;
      segment.style.left = `${left}%`;
      segment.style.width = `${width}%`;
      left += width;
      bar.appendChild(segment);
    }

    const value = document.createElement("span");
    value.textContent = `${cpu.total.toFixed(0)}%`;
    value.className = "numeric";

    row.append(name, bar, value);
    return row;
  }));
}

function renderHeader(): void {
  els.tableHead.replaceChildren(...columns.map((column) => {
    const th = document.createElement("th");
    th.textContent = column.label;
    th.className = column.numeric ? "numeric" : "";
    th.addEventListener("click", () => {
      if (state.sortKey === column.key) {
        state.sortDirection = state.sortDirection === "asc" ? "desc" : "asc";
      } else {
        state.sortKey = column.key;
        state.sortDirection = column.numeric ? "desc" : "asc";
      }
      syncSortControls();
      renderProcesses();
    });
    return th;
  }));
}

function renderSortOptions(): void {
  els.sortSelect.replaceChildren(...columns.map((column) => {
    const option = document.createElement("option");
    option.value = column.key;
    option.textContent = column.label;
    return option;
  }));
  syncSortControls();
}

function syncSortControls(): void {
  els.sortSelect.value = state.sortKey;
  els.sortDirection.textContent = state.sortDirection === "asc" ? "↑" : "↓";
}

function compareProcess(a: ProcessInfo, b: ProcessInfo): number {
  const column = columns.find((item) => item.key === state.sortKey);
  const av = a[state.sortKey];
  const bv = b[state.sortKey];
  let result = 0;

  if (column?.numeric && typeof av === "number" && typeof bv === "number") {
    result = av - bv;
  } else {
    result = String(av ?? "").localeCompare(String(bv ?? ""), undefined, { numeric: true, sensitivity: "base" });
  }

  if (result === 0) {
    result = a.pid - b.pid;
  }
  return state.sortDirection === "asc" ? result : -result;
}

function matchesFilter(process: ProcessInfo): boolean {
  if (state.filter === "sleeping" && !["S", "D"].includes(process.state)) {
    return false;
  }
  if (state.filter === "stopped" && !["T", "t"].includes(process.state)) {
    return false;
  }
  if (state.filter === "zombie" && process.state !== "Z") {
    return false;
  }
  if (state.filter === "running" && process.state !== "R") {
    return false;
  }
  if (state.filter === "idle" && process.state !== "I") {
    return false;
  }

  const query = state.search.trim().toLowerCase();
  if (!query) {
    return true;
  }

  const haystack = [
    process.pid,
    process.tid,
    process.processPid,
    process.ppid,
    process.pgrp,
    process.session,
    process.ttyNr,
    process.user,
    process.priority,
    process.nice,
    process.threads,
    process.state,
    process.stateName,
    process.cpu.toFixed(1),
    process.mem.toFixed(1),
    process.time,
    process.comm,
    process.command,
    process.isThread ? "thread" : "process",
  ].join(" ").toLowerCase();
  return haystack.includes(query);
}

function treeCompare(a: ProcessInfo, b: ProcessInfo): number {
  if (a.isThread !== b.isThread) {
    return a.isThread ? 1 : -1;
  }
  if (a.pid !== b.pid) {
    return a.pid - b.pid;
  }
  return a.command.localeCompare(b.command, undefined, { numeric: true, sensitivity: "base" });
}

function treePrefix(depth: number): string {
  if (depth <= 0) {
    return "";
  }
  return `${"|-  ".repeat(Math.max(0, depth - 1))}|- `;
}

function flattenTree(processes: ProcessInfo[]): ProcessInfo[] {
  const byParent = new Map<number, ProcessInfo[]>();
  const byPid = new Map<number, ProcessInfo>();
  for (const process of processes) {
    byPid.set(process.pid, process);
  }
  for (const process of processes) {
    const list = byParent.get(process.ppid) ?? [];
    list.push(process);
    byParent.set(process.ppid, list);
  }
  for (const list of byParent.values()) {
    list.sort(treeCompare);
  }

  const includeContext = state.filter !== "all" || state.search.trim() !== "";
  const visibleByFilter = new Map<number, boolean>();
  const evaluating = new Set<number>();
  const evaluated = new Set<number>();
  const isVisibleByFilter = (process: ProcessInfo): boolean => {
    if (evaluated.has(process.pid)) {
      return visibleByFilter.get(process.pid) ?? false;
    }
    if (evaluating.has(process.pid)) {
      return matchesFilter(process);
    }
    evaluating.add(process.pid);
    let visible = matchesFilter(process);
    if (includeContext) {
      for (const child of byParent.get(process.pid) ?? []) {
        visible = isVisibleByFilter(child) || visible;
      }
    }
    evaluating.delete(process.pid);
    evaluated.add(process.pid);
    visibleByFilter.set(process.pid, visible);
    return visible;
  };

  const roots = processes
    .filter((process) => !byPid.has(process.ppid) || process.pid === process.ppid)
    .sort(treeCompare);
  const result: ProcessInfo[] = [];
  const seen = new Set<number>();

  const visit = (process: ProcessInfo, depth: number): void => {
    if (seen.has(process.pid)) {
      return;
    }
    seen.add(process.pid);
    if (!isVisibleByFilter(process)) {
      return;
    }
    const children = byParent.get(process.pid) ?? [];
    result.push({
      ...process,
      depth,
      treePrefix: treePrefix(depth),
      hasChildren: children.length > 0,
      childCount: children.length,
      rowMatches: matchesFilter(process),
    });
    if (state.collapsedPids.has(process.pid)) {
      return;
    }
    for (const child of byParent.get(process.pid) ?? []) {
      visit(child, depth + 1);
    }
  };

  for (const root of roots) {
    visit(root, 0);
  }
  for (const process of [...processes].sort(treeCompare)) {
    visit(process, 0);
  }
  return result;
}

function processRows(): ProcessInfo[] {
  if (!state.snapshot) {
    return [];
  }
  const processes = [...state.snapshot.processes];
  const filtered = processes.filter(matchesFilter);
  return state.tree ? flattenTree(processes) : filtered.sort(compareProcess);
}

function renderProcesses(): void {
  const rows = processRows();
  const now = Date.now();

  els.processRows.replaceChildren(...rows.map((process) => {
    const tr = document.createElement("tr");
    tr.dataset.pid = String(process.pid);
    tr.classList.toggle("selected", state.selectedPid === process.pid);
    tr.classList.toggle("new-process", state.highlightNew && (state.newUntil.get(process.pid) ?? 0) > now);
    tr.classList.toggle("thread-row", process.isThread);
    tr.classList.toggle("tree-context", state.tree && process.rowMatches === false);

    tr.addEventListener("click", () => {
      state.selectedPid = process.pid;
      renderProcesses();
    });
    tr.addEventListener("contextmenu", (event) => {
      event.preventDefault();
      state.selectedPid = process.pid;
      openContextMenu(event.clientX, event.clientY);
      renderProcesses();
    });

    for (const column of columns) {
      const td = document.createElement("td");
      td.className = column.numeric ? "numeric" : "";
      if (column.key === "state") {
        const pill = document.createElement("span");
        pill.className = "state-pill";
        pill.textContent = process.state;
        pill.title = process.stateName;
        td.appendChild(pill);
      } else if (column.key === "command") {
        td.className = "command-cell";
        if (state.tree) {
          const collapse = document.createElement("button");
          collapse.type = "button";
          collapse.className = "tree-collapse";
          if (process.hasChildren) {
            collapse.textContent = state.collapsedPids.has(process.pid) ? "+" : "-";
            collapse.title = state.collapsedPids.has(process.pid) ? "Развернуть группу" : "Свернуть группу";
            collapse.addEventListener("click", (event) => {
              event.stopPropagation();
              if (state.collapsedPids.has(process.pid)) {
                state.collapsedPids.delete(process.pid);
              } else {
                state.collapsedPids.add(process.pid);
              }
              renderProcesses();
            });
          } else {
            collapse.disabled = true;
            collapse.textContent = "";
          }
          td.appendChild(collapse);
          const prefix = document.createElement("span");
          prefix.className = "tree-prefix";
          prefix.textContent = process.treePrefix ?? "";
          td.appendChild(prefix);
        }
        if (process.isThread) {
          const badge = document.createElement("span");
          badge.className = "thread-badge";
          badge.textContent = "thread";
          td.appendChild(badge);
        }
        td.append(document.createTextNode(column.format ? column.format(process) : String(process[column.key] ?? "")));
        td.title = process.isThread
          ? `Thread ${process.tid} of PID ${process.processPid}: ${process.command}`
          : process.command;
      } else {
        td.textContent = column.format ? column.format(process) : String(process[column.key] ?? "");
      }
      tr.appendChild(td);
    }
    return tr;
  }));

  if (state.snapshot) {
    const total = state.snapshot.processes.length;
    const suffix = state.showThreads ? " с потоками" : "";
    els.statusLine.textContent = `${rows.length}/${total} Tasks${suffix}`;
  }
}

async function loadSnapshot(): Promise<void> {
  try {
    const response = await fetch(state.showThreads ? "/api/snapshot?threads=1" : "/api/snapshot", { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const snapshot = await response.json() as Snapshot;
    updateNewProcesses(snapshot);
    state.snapshot = snapshot;
    setJar(snapshot.memory, snapshot.swap);
    renderCpus(snapshot.cpus);
    renderProcesses();
  } catch (error) {
    showToast("Ошибка обновления", error instanceof Error ? error.message : String(error), true);
    els.statusLine.textContent = "Ошибка подключения";
  } finally {
    window.clearTimeout(state.timer);
    if (!state.paused) {
      state.timer = window.setTimeout(loadSnapshot, state.refreshMs);
    }
  }
}

function updateNewProcesses(snapshot: Snapshot): void {
  const current = new Set(snapshot.processes.map((process) => process.pid));
  const hasPrevious = state.previousPids.size > 0;
  const until = Date.now() + 2500;
  if (hasPrevious) {
    for (const pid of current) {
      if (!state.previousPids.has(pid)) {
        state.newUntil.set(pid, until);
      }
    }
  }
  for (const pid of [...state.newUntil.keys()]) {
    if (!current.has(pid)) {
      state.newUntil.delete(pid);
    }
  }
  state.previousPids = current;
}

async function loadSignals(): Promise<void> {
  const response = await fetch("/api/signals", { cache: "no-store" });
  const payload = await response.json() as { signals: LinuxSignal[] };
  state.signals = payload.signals;
}

async function sendSignal(pid: number, signal: number): Promise<void> {
  const response = await fetch("/api/signal", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ pid, signal }),
  });
  const result = await response.json() as SignalResponse;
  if (!result.ok) {
    showToast(
      `Не получилось отправить ${result.signalName ?? `signal ${signal}`}`,
      `PID ${pid}: ${result.error ?? "unknown error"}`,
      true,
    );
    return;
  }
  showToast("Сигнал отправлен", `PID ${pid}: ${result.signalName ?? signal}`);
  await loadSnapshot();
}

function openContextMenu(x: number, y: number): void {
  els.contextMenu.hidden = false;
  const rect = els.contextMenu.getBoundingClientRect();
  els.contextMenu.style.left = `${Math.min(x, window.innerWidth - rect.width - 8)}px`;
  els.contextMenu.style.top = `${Math.min(y, window.innerHeight - rect.height - 8)}px`;
}

function closeContextMenu(): void {
  els.contextMenu.hidden = true;
}

function openSignalDialog(pid: number): void {
  state.signalTargetPid = pid;
  els.signalTitle.textContent = `Сигнал для PID ${pid}`;
  els.signalSearch.value = "";
  renderSignals();
  els.signalDialog.showModal();
  els.signalSearch.focus();
}

function renderSignals(): void {
  const query = els.signalSearch.value.trim().toLowerCase();
  const items = state.signals.filter((signal) => {
    const text = `${signal.number} ${signal.name} ${signal.description}`.toLowerCase();
    return text.includes(query);
  });
  els.signalList.replaceChildren(...items.map((signal) => {
    const item = document.createElement("button");
    item.type = "button";
    item.className = "signal-item";
    item.innerHTML = `<strong>${signal.number}</strong><span>${signal.name}</span><span>${signal.description}</span>`;
    item.addEventListener("click", async () => {
      if (state.signalTargetPid !== null) {
        els.signalDialog.close();
        await sendSignal(state.signalTargetPid, signal.number);
      }
    });
    return item;
  }));
}

function showToast(title: string, message: string, error = false): void {
  const toast = document.createElement("div");
  toast.className = `toast${error ? " error" : ""}`;
  toast.innerHTML = `<strong>${title}</strong><span>${message}</span>`;
  els.toastStack.appendChild(toast);
  window.setTimeout(() => toast.remove(), error ? 6500 : 3200);
}

function selectedPid(): number | null {
  if (state.selectedPid === null) {
    showToast("Процесс не выбран", "Выберите строку процесса", true);
    return null;
  }
  const process = state.snapshot?.processes.find((item) => item.pid === state.selectedPid);
  if (!process) {
    showToast("Процесс исчез", `PID ${state.selectedPid} больше не найден`, true);
    return null;
  }
  return process?.signalPid ?? state.selectedPid;
}

function setProcessFocus(enabled: boolean): void {
  state.processFocus = enabled;
  document.body.classList.toggle("process-focus", enabled);
  els.processFocusToggle.textContent = enabled ? "Обычный вид" : "На весь экран";
}

function bindEvents(): void {
  els.themeToggle.addEventListener("click", () => {
    const light = document.body.classList.toggle("theme-light");
    document.body.classList.toggle("theme-pink", !light);
    els.themeToggle.textContent = light ? "Розовая тема" : "Белая тема";
  });

  els.pauseToggle.addEventListener("click", () => {
    state.paused = !state.paused;
    els.pauseToggle.textContent = state.paused ? "Продолжить" : "Пауза";
    if (!state.paused) {
      void loadSnapshot();
    } else {
      window.clearTimeout(state.timer);
    }
  });

  els.refreshInput.addEventListener("change", () => {
    const value = Number(els.refreshInput.value);
    state.refreshMs = Math.max(250, Math.min(10000, Number.isFinite(value) ? value : 1000));
    els.refreshInput.value = String(state.refreshMs);
    if (!state.paused) {
      window.clearTimeout(state.timer);
      state.timer = window.setTimeout(loadSnapshot, state.refreshMs);
    }
  });

  els.sortSelect.addEventListener("change", () => {
    state.sortKey = els.sortSelect.value as keyof ProcessInfo;
    renderProcesses();
  });

  els.sortDirection.addEventListener("click", () => {
    state.sortDirection = state.sortDirection === "asc" ? "desc" : "asc";
    syncSortControls();
    renderProcesses();
  });

  els.stateFilters.addEventListener("click", (event) => {
    const target = event.target as HTMLElement;
    const button = target.closest<HTMLButtonElement>("button[data-state]");
    if (!button) {
      return;
    }
    state.filter = button.dataset.state as StateFilter;
    for (const item of els.stateFilters.querySelectorAll("button")) {
      item.classList.toggle("active", item === button);
    }
    renderProcesses();
  });

  els.treeToggle.addEventListener("change", () => {
    state.tree = els.treeToggle.checked;
    renderProcesses();
  });

  els.threadsToggle.addEventListener("change", () => {
    state.showThreads = els.threadsToggle.checked;
    state.selectedPid = null;
    state.previousPids.clear();
    state.newUntil.clear();
    void loadSnapshot();
  });

  els.highlightToggle.addEventListener("change", () => {
    state.highlightNew = els.highlightToggle.checked;
    renderProcesses();
  });

  els.processFocusToggle.addEventListener("click", () => {
    setProcessFocus(!state.processFocus);
  });

  els.searchInput.addEventListener("input", () => {
    state.search = els.searchInput.value;
    renderProcesses();
  });

  els.contextMenu.addEventListener("click", async (event) => {
    const target = event.target as HTMLElement;
    const action = target.closest<HTMLButtonElement>("button")?.dataset.action;
    const pid = selectedPid();
    closeContextMenu();
    if (!action || pid === null) {
      return;
    }
    if (action === "kill") {
      await sendSignal(pid, 9);
    } else if (action === "stop") {
      await sendSignal(pid, 19);
    } else if (action === "signal") {
      openSignalDialog(pid);
    }
  });

  document.addEventListener("click", (event) => {
    if (!els.contextMenu.contains(event.target as Node)) {
      closeContextMenu();
    }
  });

  document.addEventListener("keydown", async (event) => {
    const active = document.activeElement;
    const typing = active instanceof HTMLInputElement || active instanceof HTMLSelectElement || active instanceof HTMLTextAreaElement;
    if (typing) {
      return;
    }
    if (event.key === "Delete") {
      event.preventDefault();
      const pid = selectedPid();
      if (pid !== null) {
        await sendSignal(pid, 9);
      }
    } else if (event.key === "k" || event.key === "K") {
      const pid = selectedPid();
      if (pid !== null) {
        await sendSignal(pid, 9);
      }
    } else if (event.key === "s" || event.key === "S") {
      const pid = selectedPid();
      if (pid !== null) {
        await sendSignal(pid, 19);
      }
    } else if (event.key === "g" || event.key === "G") {
      const pid = selectedPid();
      if (pid !== null) {
        openSignalDialog(pid);
      }
    } else if (event.key === "Escape" && state.processFocus) {
      setProcessFocus(false);
    } else if (event.key === "f" || event.key === "F") {
      setProcessFocus(!state.processFocus);
    }
  });

  els.signalSearch.addEventListener("input", renderSignals);
}

async function init(): Promise<void> {
  renderHeader();
  renderSortOptions();
  bindEvents();
  await loadSignals();
  await loadSnapshot();
}

void init();
