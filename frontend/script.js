const socket = io();

// Handle Pre-Launch Setter Form Submission
document
  .getElementById("setterForm")
  .addEventListener("submit", async function (e) {
    e.preventDefault();

    const payload = {
      latitude: parseFloat(document.getElementById("latitude").value),
      longitude: parseFloat(document.getElementById("longitude").value),
      altitude: parseFloat(document.getElementById("altitude").value),
      fuze_mode: document.getElementById("fuze_mode").value,
      hob: parseFloat(document.getElementById("hob").value),
    };

    const msgElem = document.getElementById("responseMsg");
    msgElem.style.color = "#ffeb3b";
    msgElem.innerText = "Transmitting...";

    try {
      const response = await fetch("/api/set-target", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });

      const data = await response.json();

      if (data.success) {
        msgElem.style.color = "#4caf50";
        msgElem.innerText = data.message;
      } else {
        msgElem.style.color = "#f44336";
        msgElem.innerText = "Error from server!";
      }
    } catch (err) {
      console.error("Fetch error:", err);
      msgElem.style.color = "#f44336";
      msgElem.innerText = "Network error connecting to backend!";
    }
  });

// Listen for live telemetry updates from backend/ESP32
socket.on("liveTelemetry", (data) => {
  document.getElementById("status-val").innerText = data.status;
  document.getElementById("pitch-val").innerText = Number(data.pitch).toFixed(
    2,
  );
  document.getElementById("yaw-val").innerText = Number(data.yaw).toFixed(2);
  document.getElementById("g-force-val").innerText = Number(
    data.g_force,
  ).toFixed(1);

  appendLog(
    `[Telemetry] Status: ${data.status} | Pitch: ${data.pitch}° | Yaw: ${data.yaw}° | G: ${data.g_force}g`,
  );
});

// Listen for target configuration syncs
socket.on("targetUpdated", (target) => {
  document.getElementById("target-val").innerText =
    `Lat: ${target.latitude}, Lon: ${target.longitude} (${target.fuze_mode})`;
  appendLog(
    `[Target Set] Mode: ${target.fuze_mode} | Lat: ${target.latitude} | Lon: ${target.longitude}`,
  );
});

function appendLog(message) {
  const consoleDiv = document.getElementById("logConsole");
  const timestamp = new Date().toLocaleTimeString();
  consoleDiv.innerHTML += `<div>[${timestamp}] ${message}</div>`;
  consoleDiv.scrollTop = consoleDiv.scrollHeight;
}
