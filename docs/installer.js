const boards = Object.freeze({
  tdisplay: { name: "LilyGO T-Display S3", manifest: "manifests/tdisplay.json" },
  cores3: { name: "M5Stack CoreS3 / SE", manifest: "manifests/cores3.json" },
});
const installer = document.querySelector("#installer");
const button = document.querySelector("#install-button");
const consent = document.querySelector("#reset-consent");
const status = document.querySelector("#install-status");
const warning = document.querySelector("#browser-warning");
const retry = document.querySelector("#retry-check");
const supported = window.isSecureContext && "serial" in navigator;
installer.hidden = !supported;
let moduleReady = false;
let moduleFailure = "";
let verifiedBoard = null;
let pending = null;

function message(text, error = false) {
  status.textContent = moduleFailure || text;
  status.dataset.error = String(error || Boolean(moduleFailure));
}

function selectedBoard() {
  return document.querySelector('input[name="board"]:checked')?.value;
}

function updateButton() {
  const selected = selectedBoard();
  button.disabled = !supported || !moduleReady || selected !== verifiedBoard || !consent.checked;
  button.textContent = selected ? `Подключить ${boards[selected].name}` : "Подключить дисплей";
}

async function verifyFirmware() {
  pending?.abort();
  pending = new AbortController();
  const { signal } = pending;
  const selected = selectedBoard();
  verifiedBoard = null;
  retry.hidden = true;
  consent.checked = false;
  updateButton();
  if (!supported || !selected) return;
  message("Проверяю файлы прошивки…");
  try {
    const manifestUrl = new URL(boards[selected].manifest, location.href);
    const response = await fetch(manifestUrl, { signal, cache: "no-cache" });
    if (!response.ok) throw new Error("manifest");
    const manifest = await response.json();
    const build = manifest.builds?.[0];
    const part = build?.parts?.[0];
    if (manifest.board !== selected || manifest.version !== "1.0.0" ||
        manifest.builds.length !== 1 || build.chipFamily !== "ESP32-S3" ||
        build.parts.length !== 1 || part.offset !== 0 ||
        !/^[a-f0-9]{64}$/.test(manifest.sha256)) throw new Error("manifest");
    const firmwareUrl = new URL(part.path, manifestUrl);
    if (firmwareUrl.origin !== location.origin) throw new Error("origin");
    const firmware = await fetch(firmwareUrl, { signal, cache: "no-cache" });
    if (!firmware.ok) throw new Error("firmware");
    const bytes = await firmware.arrayBuffer();
    const hash = Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)),
      value => value.toString(16).padStart(2, "0")).join("");
    if (bytes.byteLength !== manifest.size || hash !== manifest.sha256) throw new Error("checksum");
    if (signal.aborted || selectedBoard() !== selected) return;
    installer.setAttribute("manifest", manifestUrl.href);
    verifiedBoard = selected;
    message(`Файлы для ${boards[selected].name} проверены. Подтвердите сброс настроек и подключите дисплей.`);
    updateButton();
  } catch (error) {
    if (signal.aborted) return;
    message("Не удалось проверить прошивку. Проверьте интернет и повторите проверку. Также можно скачать архив из релиза.", true);
    retry.hidden = false;
    updateButton();
  }
}

for (const input of document.querySelectorAll('input[name="board"]')) {
  input.addEventListener("change", verifyFirmware);
}
consent.addEventListener("change", updateButton);
retry.addEventListener("click", verifyFirmware);

if (!supported) {
  warning.hidden = false;
  warning.textContent = !window.isSecureContext
    ? "Для установки откройте эту страницу по HTTPS. Локальный файл установщика работать не будет."
    : "В этом браузере установка через USB недоступна. Откройте страницу в Chrome или Edge на компьютере.";
  message("Для установки нужен поддерживаемый браузер на компьютере.");
} else {
  import("./vendor/esp-web-tools-10.4.0/install-button.js")
    .then(() => {
      moduleReady = true;
      updateButton();
    })
    .catch(() => {
      moduleFailure = "Не удалось загрузить установщик. Обновите страницу или скачайте архив прошивки из релиза.";
      message(moduleFailure, true);
    });
}

updateButton();
