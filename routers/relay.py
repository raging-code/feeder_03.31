"""
Relay Router - Handles relay control endpoints
Manages the 2-channel relay module
"""

from flask import jsonify, request, current_app
from routers import relay_bp
from utils.esp32_client import ESP32Client
from utils.models import RelayState
import logging

logger = logging.getLogger(__name__)

def get_esp32_client():
    """Get ESP32 client instance from app config"""
    return ESP32Client(
        esp32_ip=current_app.config.get('ESP32_IP'),
        esp32_port=current_app.config.get('ESP32_PORT')
    )

@relay_bp.route('/status', methods=['GET'])
def get_relay_status():
    """
    Get current status of both relays
    Returns: Relay states
    """
    client = get_esp32_client()
    result = client.get_relay_states()
    
    if 'error' in result:
        return jsonify({'error': result['error']}), 500
    
    return jsonify(result), 200

@relay_bp.route('/<int:channel>/toggle', methods=['POST'])
def toggle_relay(channel):
    """
    Toggle a specific relay channel
    Args:
        channel: 1 or 2
    Returns: Updated relay states
    """
    if channel not in [1, 2]:
        return jsonify({'error': 'Invalid relay channel. Must be 1 or 2'}), 400
    
    client = get_esp32_client()
    result = client.toggle_relay(channel)
    
    if 'error' in result:
        return jsonify({'error': result['error']}), 500
    
    return jsonify(result), 200

@relay_bp.route('/both', methods=['POST'])
def set_both_relays():
    """
    Set both relays to same state
    Body: {'state': true/false}
    Returns: Updated relay states
    """
    data = request.get_json()
    
    if 'state' not in data:
        return jsonify({'error': 'State not provided'}), 400
    
    state = bool(data['state'])
    client = get_esp32_client()
    result = client.set_both_relays(state)
    
    if 'error' in result:
        return jsonify({'error': result['error']}), 500
    
    return jsonify(result), 200

@relay_bp.route('/<int:channel>', methods=['POST'])
def set_relay(channel):
    """
    Set specific relay state
    Args:
        channel: 1 or 2
    Body: {'state': true/false}
    Returns: Updated relay states
    """
    if channel not in [1, 2]:
        return jsonify({'error': 'Invalid relay channel. Must be 1 or 2'}), 400
    
    data = request.get_json()
    
    if 'state' not in data:
        return jsonify({'error': 'State not provided'}), 400
    
    # Since ESP32 only has toggle endpoint, we'll get current state
    # and toggle if needed
    client = get_esp32_client()
    current_state = client.get_relay_states()
    
    if 'error' in current_state:
        return jsonify({'error': current_state['error']}), 500
    
    current = current_state.get(f'relay{channel}', False)
    desired = bool(data['state'])
    
    if current != desired:
        # Toggle if current state is different from desired
        result = client.toggle_relay(channel)
        if 'error' in result:
            return jsonify({'error': result['error']}), 500
        return jsonify(result), 200
    
    return jsonify(current_state), 200