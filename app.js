// Monitor del semáforo: consulta /api/status y anima la cuenta regresiva.
const els = {
  red: document.getElementById("lampRed"),
  yellow: document.getElementById("lampYellow"),
  green: document.getElementById("lampGreen"),
  name: document.getElementById("statusName"),
  pin: document.getElementById("statusPin"),
  src: document.getElementById("statusSrc"),
  countdown: document.getElementById("countdown"),
  bar: document.getElementById("progressBar"),
  badge: document.getElementById("connBadge"),
  hint: document.getElementById("hint"),
};

const NAMES = { red: "Roja", yellow: "Amarilla", green: "Verde" };
const COLORS = { red: "#ff3b30", yellow: "#ffcc00", green: "#34c759" };

let last = null; // último estado del servidor

function render(snapshot) {
  const { light, pin, remaining, durations, connected, source, server_time, last_update } = snapshot;

  // Lámparas
  els.red.classList.toggle("lit", light === "red");
  els.yellow.classList.toggle("lit", light === "yellow");
  els.green.classList.toggle("lit", light === "green");

  els.name.textContent = NAMES[light] ?? light;
  els.name.style.color = COLORS[light] ?? "#fff";
  els.pin.textContent = "GPIO " + pin;
  els.src.textContent = source;
  els.bar.style.background = COLORS[light] ?? "#fff";

  // Cuenta regresiva interpolada entre polls (server manda remaining al momento de last_update)
  const age = Math.max(0, Date.now() / 1000 - (last_update ?? server_time));
  const live = Math.max(0, Math.ceil(remaining - age));
  els.countdown.textContent = connected ? live : "—";

  const total = durations?.[light] ?? 1;
  const frac = connected ? Math.min(1, Math.max(0, live / total)) : 0;
  els.bar.style.width = (frac * 100).toFixed(1) + "%";

  els.badge.textContent = connected ? "● en vivo · " + source : "● desconectado";
  els.badge.className = "badge " + (connected ? "badge--on" : "badge--off");
  els.hint.textContent = connected
    ? `Luz ${NAMES[light]} activa en GPIO ${pin}. Cambia en ~${live} s.`
    : "Sin señal del Arduino UNO. Revisa el USB / puerto COM.";
}

async function poll() {
  try {
    const res = await fetch("/api/status", { cache: "no-store" });
    if (!res.ok) throw new Error("HTTP " + res.status);
    last = await res.json();
    render(last);
  } catch {
    if (last) render({ ...last, connected: false });
    else {
      els.badge.textContent = "● servidor no disponible";
      els.badge.className = "badge badge--off";
    }
  }
}

// Poll al servidor + re-render suave cada 200 ms para que el conteo se vea fluido.
setInterval(poll, 1000);
setInterval(() => last && render(last), 200);
poll();
