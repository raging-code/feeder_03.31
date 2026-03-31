"""
Configuration file for the Feeder Control System
Contains all configurable parameters for the application
"""

import os

class Config:
    """Main configuration class"""
    
    # Flask settings
    SECRET_KEY = os.environ.get('SECRET_KEY') or 'dev-secret-key-change-in-production'
    
    # ESP32 Settings
    ESP32_IP = os.environ.get('ESP32_IP', '192.168.1.100')  # Default ESP32 IP - CHANGE THIS
    ESP32_PORT = os.environ.get('ESP32_PORT', 80)
    ESP32_BASE_URL = f"http://{ESP32_IP}:{ESP32_PORT}"
    
    # System Settings
    DISPENSER_COUNT = 25
    FEEDER_COUNT = 5
    DISPENSERS_PER_FEEDER = 5
    
    # Timeout settings (seconds)
    ESP32_TIMEOUT = 5
    REQUEST_RETRY_COUNT = 3
    
    # Logging settings
    LOG_LEVEL = 'INFO'
    LOG_FILE = 'feeder_system.log'
    
    # CORS settings
    CORS_ORIGINS = [
        "http://localhost:5000",
        "http://127.0.0.1:5000",
        "http://localhost:7987",
        "http://192.168.1.*"
    ]

class DevelopmentConfig(Config):
    """Development environment configuration"""
    DEBUG = True
    TESTING = False

class ProductionConfig(Config):
    """Production environment configuration"""
    DEBUG = False
    TESTING = False

class TestingConfig(Config):
    """Testing environment configuration"""
    DEBUG = True
    TESTING = True

# Select configuration based on environment
config = {
    'development': DevelopmentConfig,
    'production': ProductionConfig,
    'testing': TestingConfig,
    'default': DevelopmentConfig
}