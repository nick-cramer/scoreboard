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
const statusEl = document.getElementById("status");

async function loadScore() {
  try {
    const response = await fetch("/api/score");

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

function updateModeDisplay(mode) {
  const isAtBat = mode === "atBat";

  scoreViewEl.classList.toggle("active", !isAtBat);
  atBatViewEl.classList.toggle("active", isAtBat);
  scoreModeButtonEl.classList.toggle("active", !isAtBat);
  atBatModeButtonEl.classList.toggle("active", isAtBat);
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
