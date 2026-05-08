const homeScoreEl = document.getElementById("home-score");
const awayScoreEl = document.getElementById("away-score");
const homeNameEl = document.getElementById("home-name");
const awayNameEl = document.getElementById("away-name");
const ballsValueEl = document.getElementById("balls-value");
const strikesValueEl = document.getElementById("strikes-value");
const outsValueEl = document.getElementById("outs-value");
const inningValueEl = document.getElementById("inning-value");
const scoreViewEl = document.getElementById("score-view");
const atBatViewEl = document.getElementById("atbat-view");
const scoreModeButtonEl = document.getElementById("score-mode-button");
const atBatModeButtonEl = document.getElementById("atbat-mode-button");
const resetButtonEl = document.getElementById("reset-button");
const homeColorEl = document.getElementById("home-color");
const awayColorEl = document.getElementById("away-color");
const colorPickerEl = document.getElementById("color-picker");
const statusEl = document.getElementById("status");
let currentMode = "score";
let activeColorTeam = "";

const colorPalette = [
  "#00FF00",
  "#FF0000",
  "#FFFFFF",
  "#00B4FF",
  "#FFFF00",
  "#FF46A0",
  "#FFBE00",
  "#00F5A0",
  "#7CFF00",
  "#FF7A00",
  "#7A5CFF",
  "#00FFFF",
  "#FF8CFF",
  "#B8C4CC",
  "#145CFF",
  "#8CFFB8"
];

async function loadScore() {
  try {
    const response = await fetch("/api/connect", {
      method: "POST"
    });

    if (!response.ok) {
      throw new Error("Failed to load score");
    }

    const data = await response.json();
    updateScoreDisplay(data);
    setStatus("Ready");
  } catch (error) {
    console.error(error);
    setStatus("Connection error");
  }
}

async function changeScore(endpoint) {
  setStatus("Updating...");

  try {
    const response = await fetch(endpoint, {
      method: "POST"
    });

    if (!response.ok) {
      throw new Error("Score update failed");
    }

    const data = await response.json();
    updateScoreDisplay(data);
    setStatus("Updated");
  } catch (error) {
    console.error(error);
    setStatus("Update failed");
  }
}

async function setMode(mode) {
  setStatus("Changing view...");

  const endpoint = mode === "atBat" ? "/api/mode/atbat" : "/api/mode/score";

  try {
    const response = await fetch(endpoint, {
      method: "POST"
    });

    if (!response.ok) {
      throw new Error("Mode update failed");
    }

    const data = await response.json();
    updateScoreDisplay(data);
    setStatus("Mode changed");
  } catch (error) {
    console.error(error);
    setStatus("Mode change failed");
  }
}

async function resetCurrentView() {
  const endpoint = currentMode === "atBat" ? "/api/atbat/reset" : "/api/reset";

  await changeScore(endpoint);
}

async function setTeamColor(team, color) {
  setStatus("Updating color...");

  const endpoint = team === "home" ? "/api/home/color" : "/api/away/color";
  const body = new URLSearchParams({ color });

  try {
    const response = await fetch(endpoint, {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded"
      },
      body
    });

    if (!response.ok) {
      throw new Error("Color update failed");
    }

    const data = await response.json();
    updateScoreDisplay(data);
    closeColorPicker();
    setStatus("Color updated");
  } catch (error) {
    console.error(error);
    setStatus("Color update failed");
  }
}

async function saveTeamNames() {
  setStatus("Saving...");

  const body = new URLSearchParams({
    home: homeNameEl.value,
    away: awayNameEl.value
  });

  try {
    const response = await fetch("/api/teams", {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded"
      },
      body
    });

    if (!response.ok) {
      throw new Error("Team name update failed");
    }

    const data = await response.json();
    updateScoreDisplay(data);
    setStatus("Team names saved");
  } catch (error) {
    console.error(error);
    setStatus("Save failed");
  }
}

function updateScoreDisplay(data) {
  homeScoreEl.textContent = data.home;
  awayScoreEl.textContent = data.away;
  ballsValueEl.textContent = data.balls;
  strikesValueEl.textContent = data.strikes;
  outsValueEl.textContent = data.outs;
  inningValueEl.textContent = data.inning;

  updateTeamName(homeNameEl, data.homeName);
  updateTeamName(awayNameEl, data.awayName);
  updateTeamColor(homeColorEl, data.homeColor);
  updateTeamColor(awayColorEl, data.awayColor);
  updateModeDisplay(data.displayMode);
}

function updateTeamName(inputEl, value) {
  if (!value || document.activeElement === inputEl) {
    return;
  }

  inputEl.value = value;
}

function setStatus(message) {
  statusEl.textContent = message;
}

function updateTeamColor(swatchEl, color) {
  if (!color) {
    return;
  }

  swatchEl.style.backgroundColor = color;
}

function updateModeDisplay(mode) {
  const isAtBat = mode === "atBat";
  currentMode = isAtBat ? "atBat" : "score";

  scoreViewEl.classList.toggle("active", !isAtBat);
  atBatViewEl.classList.toggle("active", isAtBat);
  scoreModeButtonEl.classList.toggle("active", !isAtBat);
  atBatModeButtonEl.classList.toggle("active", isAtBat);
  resetButtonEl.textContent = isAtBat ? "Reset" : "Reset Score";

  if (isAtBat) {
    closeColorPicker();
  }
}

function toggleColorPicker(team) {
  if (activeColorTeam === team && colorPickerEl.classList.contains("active")) {
    closeColorPicker();
    return;
  }

  activeColorTeam = team;
  renderColorPicker();
  colorPickerEl.classList.add("active");
}

function closeColorPicker() {
  activeColorTeam = "";
  colorPickerEl.classList.remove("active");
}

function renderColorPicker() {
  colorPickerEl.innerHTML = "";

  colorPalette.forEach((color) => {
    const button = document.createElement("button");
    button.className = "palette-color";
    button.type = "button";
    button.style.backgroundColor = color;
    button.setAttribute("aria-label", color);
    button.addEventListener("click", () => setTeamColor(activeColorTeam, color));
    colorPickerEl.appendChild(button);
  });
}

homeNameEl.addEventListener("blur", saveTeamNames);
awayNameEl.addEventListener("blur", saveTeamNames);

homeNameEl.addEventListener("keydown", blurOnEnter);
awayNameEl.addEventListener("keydown", blurOnEnter);

function blurOnEnter(event) {
  if (event.key === "Enter") {
    event.preventDefault();
    event.target.blur();
  }
}

loadScore();
