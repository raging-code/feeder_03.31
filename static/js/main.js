/**
 * Feeder Control System - Main JavaScript
 * Handles UI interactions and API communication with Flask backend
 */

// Global state variables
let currentState = 'IDLE';
let dispenserStates = new Array(25).fill(true);
let updateInterval = null;

// API Base URL (Flask backend)
const API_BASE = window.location.origin + '/api';

// ESP32 Configuration
let esp32Connected = false;

/**
 * Initialize application on page load
 */
document.addEventListener('DOMContentLoaded', function() {
    console.log('Feeder Control System initialized');
    generateFeederGrid();
    loadDispenserStates();
    loadSettings();
    checkESP32Connection();
    
    // Set up periodic status updates
    updateInterval = setInterval(() => {
        updateSystemStatus();
        checkESP32Connection();
    }, 2000);
});

/**
 * Generate feeder grid HTML
 */
function generateFeederGrid() {
    const grid = document.getElementById('feedersGrid');
    grid.innerHTML = '';
    
    for (let feeder = 1; feeder <= 5; feeder++) {
        const startDisp = (feeder - 1) * 5 + 1;
        const endDisp = startDisp + 4;
        
        const column = document.createElement('div');
        column.className = 'col-md-6 col-lg mb-3';
        column.innerHTML = `
            <div class="feeder-column">
                <div class="feeder-header">
                    <h3><i class="fas fa-microchip me-2"></i>FEEDER ${feeder}</h3>
                    <p>Dispensers ${startDisp}-${endDisp}</p>
                </div>
                <div id="dispenserList${feeder}" class="p-2">
                    <!-- Dispenser boxes will be loaded here -->
                </div>
            </div>
        `;
        grid.appendChild(column);
        loadDispensersForFeeder(feeder);
    }
}

/**
 * Load dispensers for a specific feeder
 */
function loadDispensersForFeeder(feeder) {
    const container = document.getElementById(`dispenserList${feeder}`);
    if (!container) return;
    
    container.innerHTML = '';
    const startDisp = (feeder - 1) * 5 + 1;
    
    for (let d = startDisp; d < startDisp + 5; d++) {
        const dispenserNumber = d;
        const index = d - 1;
        
        const box = document.createElement('div');
        box.className = `dispenser-box ${dispenserStates[index] ? 'active' : ''}`;
        box.id = `dispenser${dispenserNumber}`;
        
        // Determine PCA and Channel info
        let pcaInfo = getDispenserPCAInfo(dispenserNumber);
        
        box.innerHTML = `
            <div class="dispenser-number">DISPENSER ${dispenserNumber}</div>
            <div class="dispenser-details">${pcaInfo}</div>
            <div class="button-group">
                <button class="feed-btn ${dispenserStates[index] ? 'active' : ''}" 
                        onclick="setDispenserState(${index}, true)">
                    <i class="fas fa-check me-1"></i>FEED
                </button>
                <button class="dont-feed-btn ${!dispenserStates[index] ? 'active' : ''}" 
                        onclick="setDispenserState(${index}, false)">
                    <i class="fas fa-times me-1"></i>DON'T FEED
                </button>
            </div>
        `;
        container.appendChild(box);
    }
}

/**
 * Get PCA and Channel info for dispenser
 */
function getDispenserPCAInfo(dispenserNumber) {
    // Based on the original ESP32 channel mapping
    if (dispenserNumber <= 11) {
        // Dispensers 1-11 on PCA9685 #1
        let channel = dispenserNumber + 4; // Channels 5-15
        return `<i class="fas fa-microchip"></i> PCA#1 Ch ${channel}`;
    } else {
        // Dispensers 12-25 on PCA9685 #2
        let channel;
        switch(dispenserNumber) {
            case 12: channel = 0; break;
            case 13: channel = 14; break;
            case 14: channel = 2; break;
            case 15: channel = 15; break;
            case 16: channel = 4; break;
            case 17: channel = 5; break;
            case 18: channel = 6; break;
            case 19: channel = 7; break;
            case 20: channel = 8; break;
            case 21: channel = 9; break;
            case 22: channel = 10; break;
            case 23: channel = 11; break;
            case 24: channel = 12; break;
            case 25: channel = 13; break;
            default: channel = 0;
        }
        return `<i class="fas fa-microchip"></i> PCA#2 Ch ${channel}`;
    }
}

/**
 * Set dispenser feeding state
 */
async function setDispenserState(index, state) {
    dispenserStates[index] = state;
    
    // Update UI
    const box = document.getElementById(`dispenser${index + 1}`);
    if (box) {
        const feedBtn = box.querySelector('.feed-btn');
        const dontBtn = box.querySelector('.dont-feed-btn');
        
        if (state) {
            feedBtn.classList.add('active');
            dontBtn.classList.remove('active');
            box.classList.add('active');
        } else {
            feedBtn.classList.remove('active');
            dontBtn.classList.add('active');
            box.classList.remove('active');
        }
    }
    
    // Send to backend
    try {
        const response = await fetch(`${API_BASE}/control/dispensers/${index}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ state: state })
        });
        
        if (!response.ok) {
            showToast('Failed to update dispenser state', 'error');
        }
    } catch (error) {
        console.error('Error setting dispenser state:', error);
        showToast('Communication error', 'error');
    }
}

/**
 * Load all dispenser states from ESP32
 */
async function loadDispenserStates() {
    try {
        const response = await fetch(`${API_BASE}/control/dispensers`);
        if (response.ok) {
            const data = await response.json();
            if (data.states) {
                dispenserStates = data.states;
                // Regenerate UI with loaded states
                generateFeederGrid();
            }
        }
    } catch (error) {
        console.error('Error loading dispenser states:', error);
    }
}

/**
 * Start feeding process
 */
async function startFeeding() {
    try {
        const response = await fetch(`${API_BASE}/control/start`, { method: 'POST' });
        if (response.ok) {
            showToast('Feeding process started', 'success');
            updateUI('RUNNING', 'All feeders operating simultaneously - sequential dispensers');
        } else {
            showToast('Failed to start feeding', 'error');
        }
    } catch (error) {
        console.error('Error starting feeding:', error);
        showToast('Communication error', 'error');
    }
}

/**
 * Stop feeding process
 */
async function stopFeeding() {
    try {
        const response = await fetch(`${API_BASE}/control/stop`, { method: 'POST' });
        if (response.ok) {
            showToast('Feeding process stopped', 'success');
            updateUI('IDLE', 'Idle');
        } else {
            showToast('Failed to stop feeding', 'error');
        }
    } catch (error) {
        console.error('Error stopping feeding:', error);
        showToast('Communication error', 'error');
    }
}

/**
 * Pause/resume feeding process
 */
async function pauseFeeding() {
    try {
        const response = await fetch(`${API_BASE}/control/pause`, { method: 'POST' });
        if (response.ok) {
            await updateSystemStatus();
            const newState = currentState === 'PAUSED' ? 'RESUMED' : 'PAUSED';
            showToast(`Feeding ${newState}`, 'info');
        }
    } catch (error) {
        console.error('Error pausing feeding:', error);
        showToast('Communication error', 'error');
    }
}

/**
 * Update system status display
 */
async function updateSystemStatus() {
    try {
        const response = await fetch(`${API_BASE}/control/status`);
        if (response.ok) {
            const data = await response.json();
            if (data.state) {
                updateUI(data.state, data.operation || '');
            }
        }
    } catch (error) {
        console.error('Error updating status:', error);
    }
}

/**
 * Update UI based on system state
 */
function updateUI(state, operation) {
    currentState = state;
    const statusElement = document.getElementById('systemStatus');
    const operationElement = document.getElementById('currentOperation');
    const startBtn = document.getElementById('startBtn');
    const stopBtn = document.getElementById('stopBtn');
    const pauseBtn = document.getElementById('pauseBtn');
    
    if (statusElement) {
        statusElement.textContent = state;
        statusElement.className = `badge ${getStatusClass(state)}`;
    }
    
    if (operationElement) {
        operationElement.textContent = operation || 'Idle';
    }
    
    if (startBtn) startBtn.disabled = state !== 'IDLE';
    if (stopBtn) stopBtn.disabled = state === 'IDLE';
    if (pauseBtn) {
        pauseBtn.disabled = state === 'IDLE';
        pauseBtn.innerHTML = state === 'PAUSED' ? 
            '<i class="fas fa-play me-2"></i>RESUME' : 
            '<i class="fas fa-pause me-2"></i>PAUSE/RESUME';
    }
}

/**
 * Get status badge class based on state
 */
function getStatusClass(state) {
    switch(state) {
        case 'IDLE': return 'bg-secondary';
        case 'RUNNING': return 'bg-warning';
        case 'PAUSED': return 'bg-info';
        default: return 'bg-secondary';
    }
}

/**
 * Save system settings
 */
async function saveSettings() {
    const settings = {
        feederInterval: parseInt(document.getElementById('feederInterval').value),
        feederAngle: parseInt(document.getElementById('feederAngle').value),
        dispenserOpenTime: parseInt(document.getElementById('dispenserOpenTime').value),
        dispenserAngle: parseInt(document.getElementById('dispenserAngle').value),
        dispenserClosedAngle: parseInt(document.getElementById('dispenserClosedAngle').value),
        intervalBetween: parseInt(document.getElementById('intervalBetween').value),
        intervalDispensers: parseInt(document.getElementById('intervalDispensers').value)
    };
    
    try {
        const response = await fetch(`${API_BASE}/settings/`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(settings)
        });
        
        if (response.ok) {
            showToast('Settings saved successfully', 'success');
            const statusDiv = document.getElementById('settingsStatus');
            if (statusDiv) {
                statusDiv.innerHTML = '<div class="alert alert-success">Settings saved!</div>';
                setTimeout(() => statusDiv.innerHTML = '', 3000);
            }
        } else {
            showToast('Failed to save settings', 'error');
        }
    } catch (error) {
        console.error('Error saving settings:', error);
        showToast('Communication error', 'error');
    }
}

/**
 * Load settings from ESP32
 */
async function loadSettings() {
    try {
        const response = await fetch(`${API_BASE}/settings/`);
        if (response.ok) {
            const settings = await response.json();
            document.getElementById('feederInterval').value = settings.feederInterval || 1000;
            document.getElementById('feederAngle').value = settings.feederAngle || 50;
            document.getElementById('dispenserOpenTime').value = settings.dispenserOpenTime || 2000;
            document.getElementById('dispenserAngle').value = settings.dispenserAngle || 0;
            document.getElementById('dispenserClosedAngle').value = settings.dispenserClosedAngle || 70;
            document.getElementById('intervalBetween').value = settings.intervalBetween || 1000;
            document.getElementById('intervalDispensers').value = settings.intervalDispensers || 1000;
        }
    } catch (error) {
        console.error('Error loading settings:', error);
    }
}

/**
 * Toggle relay
 */
async function toggleRelay(channel) {
    try {
        const response = await fetch(`${API_BASE}/relay/${channel}/toggle`, { method: 'POST' });
        if (response.ok) {
            const data = await response.json();
            updateRelayUI(channel, data[`relay${channel}`]);
            showToast(`Relay ${channel} toggled`, 'info');
        }
    } catch (error) {
        console.error('Error toggling relay:', error);
        showToast('Communication error', 'error');
    }
}

/**
 * Set both relays
 */
async function setBothRelays(state) {
    try {
        const response = await fetch(`${API_BASE}/relay/both`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ state: state })
        });
        
        if (response.ok) {
            const data = await response.json();
            updateRelayUI(1, data.relay1);
            updateRelayUI(2, data.relay2);
            showToast(`Both relays turned ${state ? 'ON' : 'OFF'}`, 'info');
        }
    } catch (error) {
        console.error('Error setting both relays:', error);
        showToast('Communication error', 'error');
    }
}

/**
 * Update relay UI
 */
function updateRelayUI(channel, isOn) {
    const indicator = document.getElementById(`relay${channel}Indicator`);
    const statusSpan = document.getElementById(`relay${channel}Status`);
    const btn = document.getElementById(`relay${channel}Btn`);
    
    if (indicator) {
        indicator.textContent = isOn ? 'ON' : 'OFF';
        indicator.className = `badge ${isOn ? 'bg-success' : 'bg-secondary'}`;
    }
    
    if (statusSpan) {
        statusSpan.textContent = isOn ? 'ON' : 'OFF';
    }
    
    if (btn) {
        btn.innerHTML = isOn ? 
            '<i class="fas fa-power-off me-2"></i>Turn OFF' : 
            '<i class="fas fa-power-off me-2"></i>Turn ON';
        btn.className = `btn btn-lg ${isOn ? 'btn-danger' : 'btn-success'}`;
    }
}

/**
 * Check ESP32 connection status
 */
async function checkESP32Connection() {
    try {
        const response = await fetch(`${API_BASE}/control/status`);
        if (response.ok) {
            esp32Connected = true;
            const statusElement = document.getElementById('esp32Status');
            if (statusElement) {
                statusElement.textContent = 'Connected';
                statusElement.className = 'badge bg-success';
            }
        } else {
            setESP32Disconnected();
        }
    } catch (error) {
        setESP32Disconnected();
    }
}

/**
 * Set ESP32 as disconnected
 */
function setESP32Disconnected() {
    esp32Connected = false;
    const statusElement = document.getElementById('esp32Status');
    if (statusElement) {
        statusElement.textContent = 'Disconnected';
        statusElement.className = 'badge bg-danger';
    }
}

/**
 * Show toast notification
 */
function showToast(message, type = 'info') {
    // Create toast container if not exists
    let container = document.querySelector('.toast-container');
    if (!container) {
        container = document.createElement('div');
        container.className = 'toast-container';
        document.body.appendChild(container);
    }
    
    const toastId = 'toast-' + Date.now();
    const bgClass = type === 'success' ? 'bg-success' : type === 'error' ? 'bg-danger' : 'bg-info';
    
    const toast = document.createElement('div');
    toast.className = `toast ${bgClass} text-white`;
    toast.id = toastId;
    toast.setAttribute('role', 'alert');
    toast.setAttribute('aria-live', 'assertive');
    toast.setAttribute('aria-atomic', 'true');
    
    toast.innerHTML = `
        <div class="toast-header">
            <strong class="me-auto">Feeder System</strong>
            <small>Just now</small>
            <button type="button" class="btn-close" data-bs-dismiss="toast"></button>
        </div>
        <div class="toast-body">
            ${message}
        </div>
    `;
    
    container.appendChild(toast);
    const bsToast = new bootstrap.Toast(toast);
    bsToast.show();
    
    toast.addEventListener('hidden.bs.toast', () => {
        toast.remove();
    });
}