const socket = io();

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

socket.on("liveTelemetry", (data) => {
  const box = document.getElementById("telemetryDisplay");
  box.innerHTML = `
        <p><strong>Status:</strong> <span class="status-armed">${data.status}</span></p>
        <p><strong>Pitch Angle:</strong> ${data.pitch.toFixed(2)} °</p>
        <p><strong>Yaw Angle:</strong> ${data.yaw.toFixed(2)} °</p>
        <p><strong>Launch G-Force:</strong> ${data.g_force.toFixed(1)} g</p>
    `;
});
