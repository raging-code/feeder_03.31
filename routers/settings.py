"""
Settings Router - Handles system configuration endpoints
Manages feeder, dispenser, and timing settings
"""

from flask import jsonify, request, current_app
from routers import settings_bp
from utils.esp32_client import ESP32Client
from utils.models import SystemSettings, FeederSettings, DispenserSettings, TimingSettings
import logging

logger = logging.getLogger(__name__)

def get_esp32_client():
    """Get ESP32 client instance from app config"""
    return ESP32Client(
        esp32_ip=current_app.config.get('ESP32_IP'),
        esp32_port=current_app.config.get('ESP32_PORT')
    )

@settings_bp.route('/', methods=['GET'])
def get_settings():
    """
    Get current system settings
    Returns: All configuration settings
    """
    client = get_esp32_client()
    result = client.get_settings()
    
    if 'error' in result:
        return jsonify({'error': result['error']}), 500
    
    return jsonify(result), 200

@settings_bp.route('/', methods=['POST'])
def save_settings():
    """
    Save system settings to ESP32
    Body: Settings JSON object
    """
    data = request.get_json()
    
    # Validate settings
    required_fields = ['feederInterval', 'feederAngle', 'dispenserOpenTime', 
                      'dispenserAngle', 'dispenserClosedAngle', 'intervalBetween', 
                      'intervalDispensers']
    
    for field in required_fields:
        if field not in data:
            return jsonify({'error': f'Missing required field: {field}'}), 400
    
    # Constrain values to valid ranges
    data['feederInterval'] = max(100, min(10000, data.get('feederInterval', 1000)))
    data['feederAngle'] = max(0, min(180, data.get('feederAngle', 50)))
    data['dispenserOpenTime'] = max(1000, min(30000, data.get('dispenserOpenTime', 2000)))
    data['dispenserAngle'] = max(0, min(180, data.get('dispenserAngle', 0)))
    data['dispenserClosedAngle'] = max(0, min(180, data.get('dispenserClosedAngle', 70)))
    data['intervalBetween'] = max(100, min(5000, data.get('intervalBetween', 1000)))
    data['intervalDispensers'] = max(100, min(5000, data.get('intervalDispensers', 1000)))
    
    client = get_esp32_client()
    result = client.save_settings(data)
    
    if 'error' in result:
        return jsonify({'error': result['error']}), 500
    
    return jsonify({'success': True, 'message': 'Settings saved successfully'}), 200

@settings_bp.route('/defaults', methods=['GET'])
def get_default_settings():
    """
    Get default system settings
    Returns: Default configuration values
    """
    feeder = FeederSettings()
    dispenser = DispenserSettings()
    timing = TimingSettings()
    settings = SystemSettings(feeder, dispenser, timing)
    
    return jsonify(settings.to_dict()), 200