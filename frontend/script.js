const socket = io();

// Initialize Chart.js graph
const ctx = document.getElementById("telemetryChart").getContext("2d");
const telemetryChart = new Chart(ctx, {
  type: "line",
  data: {
    labels: [],
    datasets: [
      {
        label: "Pitch (°)",
        data: [],
        borderColor: "#4caf50",
        backgroundColor: "rgba(76, 175, 80, 0.1)",
        borderWidth: 2,
        tension: 0.2,
      },
      {
        label: "Yaw (°)",
        data: [],
        borderColor: "#2196f3",
        backgroundColor: "rgba(33, 150, 243, 0.1)",
        borderWidth: 2,
        tension: 0.2,
      },
    ],
  },
  options: {
    responsive: true,
    animation: false,
    scales: {
      y: { grid: { color: "#222" }, ticks: { color: "#aaa" } },
      x: { grid: { color: "#222" }, ticks: { color: "#aaa" } },
    },
    plugins: {
      legend: { labels: { color: "#fff", font: { size: 11 } } },
    },
  },
});

// Dynamic fuze mode input behavior
const fuzeModeSelect = document.getElementById("fuze_mode");
if (fuzeModeSelect) {
  fuzeModeSelect.addEventListener("change", (e) => {
    const mode = e.target.value;
    const fuzeTimeGroup = document.getElementById("fuze-time-group");
    const fuzeTimeLabel = document.getElementById("fuze_time_label");

    if (mode === "Proximity") {
      if (fuzeTimeGroup) fuzeTimeGroup.style.display = "block";
      if (fuzeTimeLabel)
        fuzeTimeLabel.innerText = "Time to Airburst (seconds):";
    } else if (mode === "Time") {
      if (fuzeTimeGroup) fuzeTimeGroup.style.display = "block";
      if (fuzeTimeLabel)
        fuzeTimeLabel.innerText = "Post-Impact Delay (milliseconds):";
    } else if (mode === "Impact") {
      if (fuzeTimeGroup) fuzeTimeGroup.style.display = "none";
    }
  });
}

// Handle Pre-Launch Setter Form Submission
const setterForm = document.getElementById("setterForm");
if (setterForm) {
  setterForm.addEventListener("submit", async function (e) {
    e.preventDefault();

    const fuzeTimeInput = document.getElementById("fuze_time_val");
    const fuzeTimeVal = fuzeTimeInput
      ? parseFloat(fuzeTimeInput.value) || 0
      : 0;

    const payload = {
      latitude: parseFloat(document.getElementById("latitude").value) || 0,
      longitude: parseFloat(document.getElementById("longitude").value) || 0,
      altitude: parseFloat(document.getElementById("altitude").value) || 0,
      fuze_mode: document.getElementById("fuze_mode")
        ? document.getElementById("fuze_mode").value
        : "Impact",
      hob: fuzeTimeVal,
    };

    const msgElem = document.getElementById("responseMsg");
    if (msgElem) {
      msgElem.style.color = "#ffeb3b";
      msgElem.innerText = "Transmitting...";
    }

    try {
      const response = await fetch("/api/set-target", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });

      const data = await response.json();

      if (msgElem) {
        if (data.success) {
          msgElem.style.color = "#4caf50";
          msgElem.innerText = data.message;
        } else {
          msgElem.style.color = "#f44336";
          msgElem.innerText = "Error from server!";
        }
      }
    } catch (err) {
      console.error("Fetch error:", err);
      if (msgElem) {
        msgElem.style.color = "#f44336";
        msgElem.innerText = "Network error connecting to backend!";
      }
    }
  });
}

// Listen for live telemetry updates
socket.on("liveTelemetry", (data) => {
  const statusEl = document.getElementById("status-val");
  const pitchEl = document.getElementById("pitch-val");
  const yawEl = document.getElementById("yaw-val");
  const gForceEl = document.getElementById("g-force-val");

  if (statusEl) statusEl.innerText = data.status || "--";
  if (pitchEl) pitchEl.innerText = Number(data.pitch || 0).toFixed(2);
  if (yawEl) yawEl.innerText = Number(data.yaw || 0).toFixed(2);
  if (gForceEl) gForceEl.innerText = Number(data.g_force || 0).toFixed(1);

  const c1El = document.getElementById("c1-val");
  const c2El = document.getElementById("c2-val");
  const c3El = document.getElementById("c3-val");
  const c4El = document.getElementById("c4-val");

  if (c1El) c1El.innerText = Number(data.canard_1 || 0).toFixed(1);
  if (c2El) c2El.innerText = Number(data.canard_2 || 0).toFixed(1);
  if (c3El) c3El.innerText = Number(data.canard_3 || 0).toFixed(1);
  if (c4El) c4El.innerText = Number(data.canard_4 || 0).toFixed(1);

  appendLog(
    `[Telemetry] State: ${data.status} | C1:${data.canard_1}° C2:${data.canard_2}° C3:${data.canard_3}° C4:${data.canard_4}°`,
  );

  const timeLabel = new Date().toLocaleTimeString();
  if (telemetryChart.data.labels.length > 15) {
    telemetryChart.data.labels.shift();
    telemetryChart.data.datasets[0].data.shift();
    telemetryChart.data.datasets[1].data.shift();
  }
  telemetryChart.data.labels.push(timeLabel);
  telemetryChart.data.datasets[0].data.push(data.pitch || 0);
  telemetryChart.data.datasets[1].data.push(data.yaw || 0);
  telemetryChart.update();
});

// Listen for target configuration syncs
socket.on("targetUpdated", (target) => {
  const targetEl = document.getElementById("target-val");
  if (targetEl) {
    targetEl.innerText = `Lat: ${target.latitude}, Lon: ${target.longitude} (${target.fuze_mode})`;
  }
  appendLog(
    `[Target Set] Mode: ${target.fuze_mode} | Lat: ${target.latitude} | Lon: ${target.longitude}`,
  );
});

function appendLog(message) {
  const consoleDiv = document.getElementById("logConsole");
  if (consoleDiv) {
    const timestamp = new Date().toLocaleTimeString();
    consoleDiv.innerHTML += `<div>[${timestamp}] ${message}</div>`;
    consoleDiv.scrollTop = consoleDiv.scrollHeight;
  }
}
