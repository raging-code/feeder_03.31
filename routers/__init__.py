"""
Router Blueprint Initialization
Creates and exports blueprint modules for Flask routes
"""

from flask import Blueprint

# Create blueprints for different functional areas
control_bp = Blueprint('control', __name__, url_prefix='/api/control')
settings_bp = Blueprint('settings', __name__, url_prefix='/api/settings')
relay_bp = Blueprint('relay', __name__, url_prefix='/api/relay')

# Import route modules to register endpoints
from routers import control, settings, relay

# Export blueprints for use in main app
__all__ = ['control_bp', 'settings_bp', 'relay_bp']