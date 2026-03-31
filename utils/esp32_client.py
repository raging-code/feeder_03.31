"""
ESP32 Communication Client
Handles all communication with the ESP32 microcontroller
"""

import requests
import logging
from typing import Dict, List, Any, Optional
from config import Config

# Configure logging
logger = logging.getLogger(__name__)

class ESP32Client:
    """
    Client for communicating with ESP32 microcontroller
    Handles HTTP requests to control feeders, dispensers, and relays
    """
    
    def __init__(self, esp32_ip: str = None, esp32_port: int = None):
        """
        Initialize ESP32 client with connection parameters
        
        Args:
            esp32_ip: IP address of ESP32 (overrides config)
            esp32_port: Port number of ESP32 (overrides config)
        """
        self.base_url = f"http://{esp32_ip or Config.ESP32_IP}:{esp32_port or Config.ESP32_PORT}"
        self.timeout = Config.ESP32_TIMEOUT
        self.retry_count = Config.REQUEST_RETRY_COUNT
        
    def _make_request(self, endpoint: str, method: str = 'GET', 
                      data: Optional[Dict] = None, params: Optional[Dict] = None) -> Dict:
        """
        Make HTTP request to ESP32 with retry logic
        
        Args:
            endpoint: API endpoint path
            method: HTTP method (GET, POST)
            data: JSON data for POST requests
            params: Query parameters for GET requests
            
        Returns:
            Dictionary with response data or error information
        """
        url = f"{self.base_url}/{endpoint}"
        
        for attempt in range(self.retry_count):
            try:
                if method.upper() == 'GET':
                    response = requests.get(url, params=params, timeout=self.timeout)
                elif method.upper() == 'POST':
                    response = requests.post(url, json=data, timeout=self.timeout)
                else:
                    return {'error': f'Unsupported method: {method}'}
                
                response.raise_for_status()
                
                # Try to parse JSON response
                try:
                    return response.json()
                except:
                    return {'success': True, 'message': response.text}
                    
            except requests.exceptions.RequestException as e:
                logger.warning(f"Request failed (attempt {attempt + 1}/{self.retry_count}): {e}")
                if attempt == self.retry_count - 1:
                    logger.error(f"All retry attempts failed for {url}")
                    return {'error': f'Failed to communicate with ESP32: {str(e)}'}
                    
        return {'error': 'Communication failed after all retries'}
    
    # System Control Methods
    def get_system_state(self) -> Dict:
        """Get current system state (IDLE, RUNNING, PAUSED)"""
        return self._make_request('getSystemState')
    
    def start_feeding(self) -> Dict:
        """Start the feeding process"""
        return self._make_request('startFeeding')
    
    def stop_feeding(self) -> Dict:
        """Stop the feeding process"""
        return self._make_request('stopFeeding')
    
    def pause_feeding(self) -> Dict:
        """Pause/Resume the feeding process"""
        return self._make_request('pauseFeeding')
    
    # Dispenser Control Methods
    def set_dispenser_state(self, index: int, state: bool) -> Dict:
        """
        Set individual dispenser feeding state
        
        Args:
            index: Dispenser index (0-24)
            state: True to feed, False to skip
        """
        params = {'index': index, 'state': str(state).lower()}
        return self._make_request('setDispenserState', params=params)
    
    def get_dispenser_states(self) -> Dict:
        """Get all dispenser feeding states"""
        return self._make_request('getDispenserStates')
    
    def get_dispenser_info(self) -> Dict:
        """Get dispenser channel information"""
        return self._make_request('getDispenserInfo')
    
    # Relay Control Methods
    def get_relay_states(self) -> Dict:
        """Get current relay states"""
        return self._make_request('getRelayStates')
    
    def toggle_relay(self, channel: int) -> Dict:
        """
        Toggle a specific relay channel
        
        Args:
            channel: Relay channel (1 or 2)
        """
        params = {'channel': channel}
        return self._make_request('toggleRelay', params=params)
    
    def set_both_relays(self, state: bool) -> Dict:
        """
        Set both relays to same state
        
        Args:
            state: True for ON, False for OFF
        """
        params = {'state': str(state).lower()}
        return self._make_request('setBothRelays', params=params)
    
    # Settings Methods
    def get_settings(self) -> Dict:
        """Get current system settings"""
        return self._make_request('getSettings')
    
    def save_settings(self, settings: Dict) -> Dict:
        """
        Save system settings to ESP32
        
        Args:
            settings: Dictionary with setting values
        """
        return self._make_request('saveSettings', method='POST', data=settings)
    
    # Utility Methods
    def is_connected(self) -> bool:
        """Check if ESP32 is reachable"""
        try:
            response = requests.get(f"{self.base_url}/", timeout=2)
            return response.status_code == 200
        except:
            return False