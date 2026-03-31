"""
Control Router - Handles system control endpoints
"""

from flask import jsonify, request, current_app
from routers import control_bp
from utils.esp32_client import ESP32Client
import logging

logger = logging.getLogger(__name__)

def get_esp32_client():
    return ESP32Client(
        esp32_ip=current_app.config.get('ESP32_IP'),
        esp32_port=current_app.config.get('ESP32_PORT')
    )

@control_bp.route('/status', methods=['GET'])
def get_system_status():
    client = get_esp32_client()
    result = client.get_system_state()
    if 'error' in result:
        return jsonify({'error': result['error'], 'connected': False}), 503
    return jsonify(result), 200

@control_bp.route('/start', methods=['POST'])
def start_feeding():
    client = get_esp32_client()
    result = client.start_feeding()
    if 'error' in result:
        return jsonify({'error': result['error']}), 500
    return jsonify(result), 200

@control_bp.route('/stop', methods=['POST'])
def stop_feeding():
    client = get_esp32_client()
    result = client.stop_feeding()
    if 'error' in result:
        return jsonify({'error': result['error']}), 500
    return jsonify(result), 200

@control_bp.route('/pause', methods=['POST'])
def pause_feeding():
    client = get_esp32_client()
    result = client.pause_feeding()
    if 'error' in result:
        return jsonify({'error': result['error']}), 500
    return jsonify(result), 200

@control_bp.route('/dispensers', methods=['GET'])
def get_dispensers():
    client = get_esp32_client()
    states_result = client.get_dispenser_states()
    info_result = client.get_dispenser_info()
    if 'error' in states_result:
        return jsonify({'error': states_result['error']}), 500
    response = {
        'states': states_result.get('states', []),
        'channels': info_result.get('channels', []) if 'error' not in info_result else []
    }
    return jsonify(response), 200

@control_bp.route('/dispensers/<int:dispenser_index>', methods=['POST'])
def set_dispenser(dispenser_index):
    if not 0 <= dispenser_index < 25:
        return jsonify({'error': 'Invalid dispenser index'}), 400
    data = request.get_json()
    if 'state' not in data:
        return jsonify({'error': 'State not provided'}), 400
    state = bool(data['state'])
    client = get_esp32_client()
    result = client.set_dispenser_state(dispenser_index, state)
    if 'error' in result:
        return jsonify({'error': result['error']}), 500
    return jsonify(result), 200