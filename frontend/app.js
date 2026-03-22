const keys = [
  "x",
  "y",
  "window_size",
  "cross_half",
  "line_width",
  "color_r",
  "color_g",
  "color_b"
];

const limits = {
  x: [-1, 10000],
  y: [-1, 10000],
  window_size: [20, 800],
  cross_half: [1, 400],
  line_width: [1, 20],
  color_r: [0, 255],
  color_g: [0, 255],
  color_b: [0, 255]
};

const $ = (id) => document.getElementById(id);

async function post(path, obj = {}) {
  const body = new URLSearchParams(obj).toString();
  const response = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body
  });

  if (!response.ok) {
    throw new Error((await response.text()) || `HTTP ${response.status}`);
  }

  const contentType = response.headers.get("content-type") || "";
  return contentType.includes("application/json")
    ? response.json()
    : response.text();
}

async function state() {
  const response = await fetch("/api/state", { cache: "no-store" });
  if (!response.ok) {
    throw new Error((await response.text()) || "state fail");
  }
  return response.json();
}

function formData() {
  const data = {};
  keys.forEach((key) => {
    data[key] = $(key).value.trim();
  });
  return data;
}

function fill(cfg) {
  keys.forEach((key) => {
    $(key).value = cfg[key];
  });
  paintSwatch();
}

function paintSwatch() {
  const r = Number($("color_r").value || 0);
  const g = Number($("color_g").value || 0);
  const b = Number($("color_b").value || 0);
  $("swatch").style.background = `rgb(${r},${g},${b})`;
}

function normName(name) {
  const trimmed = (name || "").trim();
  if (!trimmed) {
    return "";
  }
  return /\.ini$/i.test(trimmed) ? trimmed : `${trimmed}.ini`;
}

function clampByKey(key, value) {
  const [minValue, maxValue] = limits[key] || [-999999, 999999];
  return Math.max(minValue, Math.min(maxValue, value));
}

let dirty = false;
let inAutoApply = false;

function render(stateData, forceFill = false) {
  const select = $("profileSelect");
  const current = stateData.active_profile || "";
  select.innerHTML = "";

  (stateData.profiles || []).forEach((name) => {
    const option = document.createElement("option");
    option.value = name;
    option.textContent = name;
    if (name === current) {
      option.selected = true;
    }
    select.appendChild(option);
  });

  if (forceFill || !dirty) {
    $("profileName").value = current;
    fill(stateData.config);
  }

  const running = !!stateData.running;
  $("dot").classList.toggle("on", running);
  $("st").textContent = running ? "状态: 运行中" : "状态: 已停止";
  $("toggleBtn").textContent = running ? "停止准星" : "启动准星";
}

async function refresh() {
  try {
    render(await state(), false);
  } catch (error) {
    alert(`刷新失败: ${error.message}`);
  }
}

async function autoApplyNow() {
  if (inAutoApply) {
    return;
  }

  inAutoApply = true;
  try {
    const nextState = await post("/api/apply", formData());
    dirty = false;
    render(nextState, false);
  } catch (error) {
    alert(`自动应用失败: ${error.message}`);
  } finally {
    inAutoApply = false;
  }
}

keys.forEach((key) => {
  const input = $(key);
  input.addEventListener("input", () => {
    dirty = true;
    paintSwatch();
  });
  input.addEventListener("blur", () => {
    if (dirty) {
      autoApplyNow();
    }
  });
  input.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      autoApplyNow();
    }
  });
});

document.querySelectorAll("[data-step-target]").forEach((button) => {
  button.addEventListener("click", () => {
    const key = button.getAttribute("data-step-target");
    const step = Number(button.getAttribute("data-step") || 1);
    const input = $(key);
    const current = Number(input.value || 0);
    const next = clampByKey(key, Number.isFinite(current) ? current + step : step);
    input.value = String(next);
    dirty = true;
    paintSwatch();
    autoApplyNow();
  });
});

$("toggleBtn").onclick = async () => {
  try {
    const current = await state();
    render(await post("/api/toggle", { running: current.running ? 0 : 1 }), false);
  } catch (error) {
    alert(`切换失败: ${error.message}`);
  }
};

$("saveBtn").onclick = async () => {
  try {
    const nextState = await post("/api/profile/save", {
      ...formData(),
      name: $("profileName").value.trim()
    });
    dirty = false;
    render(nextState, true);
  } catch (error) {
    alert(`保存失败: ${error.message}`);
  }
};

$("newBtn").onclick = async () => {
  const name = $("profileName").value.trim();
  if (!name) {
    alert("请先输入配置名");
    return;
  }

  try {
    const nextState = await post("/api/profile/new", {
      ...formData(),
      name
    });
    dirty = false;
    render(nextState, true);
  } catch (error) {
    alert(`新建失败: ${error.message}`);
  }
};

$("renameBtn").onclick = async () => {
  const oldName = $("profileSelect").value;
  const newName = $("profileName").value.trim();
  if (!oldName) {
    alert("请先选择配置");
    return;
  }
  if (!newName) {
    alert("请先输入新配置名");
    return;
  }
  if (oldName.toLowerCase() === normName(newName).toLowerCase()) {
    alert("新旧配置名相同");
    return;
  }

  try {
    const nextState = await post("/api/profile/rename", {
      old_name: oldName,
      new_name: newName
    });
    dirty = false;
    render(nextState, true);
  } catch (error) {
    alert(`重命名失败: ${error.message}`);
  }
};

$("quitBtn").onclick = async () => {
  if (!confirm("确认退出程序？")) {
    return;
  }

  try {
    await post("/api/quit", {});
    window.close();
  } catch (error) {
    alert(`退出失败: ${error.message}`);
  }
};

$("profileSelect").addEventListener("change", async () => {
  const name = $("profileSelect").value;
  $("profileName").value = name;
  if (!name) {
    return;
  }

  try {
    const nextState = await post("/api/profile/load", { name });
    dirty = false;
    render(nextState, true);
  } catch (error) {
    alert(`自动加载失败: ${error.message}`);
  }
});

async function ping() {
  try {
    await post("/api/ping", {});
  } catch (_) {
  }
}

setInterval(ping, 1000);
window.addEventListener("beforeunload", () => {
  navigator.sendBeacon("/api/quit", "");
});
refresh();
