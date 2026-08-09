(() => {
  const form = document.querySelector("#connection-form");
  const ssidSelect = document.querySelector("#ssid");
  const passwordInput = document.querySelector("#password");
  const refreshButton = document.querySelector("#refresh");
  const status = document.querySelector("#status");

  function setStatus(message, isError = false) {
    status.textContent = message;
    status.className = isError ? "error" : "";
  }

  function fillNetworks(networks) {
    ssidSelect.replaceChildren();

    if (!networks.length) {
      const option = new Option("Nenhuma rede encontrada", "");
      ssidSelect.add(option);
      return;
    }

    networks.forEach((network) => {
      ssidSelect.add(new Option(network, network));
    });
  }

  async function loadNetworks() {
    refreshButton.disabled = true;
    setStatus("Buscando redes...");

    try {
      const response = await fetch("/api/networks", { cache: "no-store" });
      const data = await response.json();

      if (!response.ok) {
        throw new Error(data.error || "Falha ao buscar redes");
      }

      fillNetworks(Array.isArray(data.networks) ? data.networks : []);
      setStatus("Redes atualizadas.");
    } catch (error) {
      fillNetworks([]);
      setStatus(error.message, true);
    } finally {
      refreshButton.disabled = false;
    }
  }

  async function connect() {
    const ssid = ssidSelect.value;
    const password = passwordInput.value;

    if (!ssid) {
      setStatus("Selecione uma rede.", true);
      return;
    }

    setStatus("Tentando conectar...");

    try {
      const response = await fetch("/api/connect", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ssid, password })
      });
      const data = await response.json();

      if (!response.ok || !data.connected) {
        throw new Error(data.error || "Não foi possível conectar");
      }

      setStatus("Conectado. A rede AP será encerrada.");
      form.querySelector("button[type=submit]").disabled = true;
    } catch (error) {
      setStatus(error.message, true);
    }
  }

  refreshButton.addEventListener("click", loadNetworks);
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    connect();
  });

  loadNetworks();
})();
