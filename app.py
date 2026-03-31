"""
Feeder Control System - Main Flask Application
Web interface for controlling 5 feeders and 25 dispensers with relay module
"""

from flask import Flask, render_template, send_from_directory
from flask_cors import CORS
from routers import control_bp, settings_bp, relay_bp
import logging
import os

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

def create_app(config_name='default'):
    """
    Application factory pattern for creating Flask app
    Args:
        config_name: Configuration environment ('development', 'production', 'testing')
    Returns:
        Flask application instance
    """
    app = Flask(__name__)
    
    # Load configuration
    from config import config
    app.config.from_object(config[config_name])
    
    # Initialize CORS
    CORS(app, 
         supports_credentials=True,
         origins=app.config.get('CORS_ORIGINS', []))
    
    # Register blueprints
    app.register_blueprint(control_bp)
    app.register_blueprint(settings_bp)
    app.register_blueprint(relay_bp)
    
    # Set ESP32 configuration from environment
    app.config['ESP32_IP'] = os.environ.get('ESP32_IP', app.config.get('ESP32_IP', '192.168.1.100'))
    app.config['ESP32_PORT'] = int(os.environ.get('ESP32_PORT', app.config.get('ESP32_PORT', 80)))
    
    logger.info(f"ESP32 configured at: {app.config['ESP32_IP']}:{app.config['ESP32_PORT']}")
    
    return app

# Create app instance
app = create_app(os.environ.get('FLASK_ENV', 'development'))

@app.route('/')
def index():
    """
    Main dashboard route
    Renders the main control interface
    """
    return render_template('index.html')

@app.route('/static/<path:path>')
def serve_static(path):
    """
    Serve static files (CSS, JS)
    Args:
        path: File path relative to static directory
    """
    return send_from_directory('static', path)

@app.errorhandler(404)
def not_found(error):
    """Handle 404 errors"""
    return {'error': 'Resource not found'}, 404

@app.errorhandler(500)
def internal_error(error):
    """Handle 500 errors"""
    logger.error(f"Internal server error: {error}")
    return {'error': 'Internal server error'}, 500

if __name__ == '__main__':
    # Get port from environment or use default
    port = int(os.environ.get('PORT', 5000))
    
    # Run the Flask application
    app.run(
        host='0.0.0.0',
        port=port,
        debug=app.config.get('DEBUG', False),
        threaded=True
    )