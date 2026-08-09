(() => {
  const distanceElement = document.querySelector("#distance");
  const status = document.querySelector("#status");
  const POLL_INTERVAL_MS = 5000;

  function setStatus(message, isError = false) {
    status.textContent = message;
    status.className = isError ? "error" : "";
  }

  async function loadDistance() {
    try {
      const response = await fetch("/api/distance", { cache: "no-store" });
      const data = await response.json();

      if (!response.ok) {
        throw new Error(data.error || "Falha ao buscar a distância");
      }

      if (typeof data.distance !== "number") {
        distanceElement.textContent = "--";
        setStatus("Leitura indisponível", true);
        return;
      }

      distanceElement.textContent = data.distance.toFixed(2);
      setStatus("Leitura atualizada.");
    } catch (error) {
      setStatus(error.message || "Falha ao buscar a distância", true);
    }
  }

  loadDistance();
  window.setInterval(loadDistance, POLL_INTERVAL_MS);
})();
