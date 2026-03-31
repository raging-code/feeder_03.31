"""
Data models for the Feeder Control System
Defines data structures used throughout the application
"""

from dataclasses import dataclass
from typing import List, Optional
from enum import Enum

class SystemState(Enum):
    """System operating states"""
    IDLE = "IDLE"
    RUNNING = "RUNNING"
    PAUSED = "PAUSED"
    
    @classmethod
    def from_string(cls, value: str):
        """Convert string to SystemState enum"""
        try:
            return cls(value.upper())
        except:
            return cls.IDLE

@dataclass
class SystemStatus:
    """System status information"""
    state: SystemState
    operation: str
    connected: bool = True
    
    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization"""
        return {
            'state': self.state.value,
            'operation': self.operation,
            'connected': self.connected
        }

@dataclass
class Dispenser:
    """Dispenser data model"""
    number: int
    feeder: int
    pca: int
    channel: int
    feed_state: bool
    
    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization"""
        return {
            'number': self.number,
            'feeder': self.feeder,
            'pca': self.pca,
            'channel': self.channel,
            'feed_state': self.feed_state
        }

@dataclass
class FeederSettings:
    """Feeder configuration settings"""
    return_time: int = 1000      # milliseconds
    angle: int = 50              # degrees (0-180)
    
    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization"""
        return {
            'feederInterval': self.return_time,
            'feederAngle': self.angle
        }

@dataclass
class DispenserSettings:
    """Dispenser configuration settings"""
    open_time: int = 2000        # milliseconds
    open_angle: int = 0          # degrees (0-180)
    closed_angle: int = 70       # degrees (0-180)
    
    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization"""
        return {
            'dispenserOpenTime': self.open_time,
            'dispenserAngle': self.open_angle,
            'dispenserClosedAngle': self.closed_angle
        }

@dataclass
class TimingSettings:
    """Timing configuration settings"""
    dispenser_to_feeder_delay: int = 1000   # milliseconds
    cycle_interval: int = 1000              # milliseconds
    
    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization"""
        return {
            'intervalBetween': self.dispenser_to_feeder_delay,
            'intervalDispensers': self.cycle_interval
        }

@dataclass
class SystemSettings:
    """Complete system settings"""
    feeder: FeederSettings
    dispenser: DispenserSettings
    timing: TimingSettings
    
    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization"""
        return {
            **self.feeder.to_dict(),
            **self.dispenser.to_dict(),
            **self.timing.to_dict()
        }

@dataclass
class RelayState:
    """Relay state information"""
    channel: int
    is_on: bool
    
    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization"""
        return {
            f'relay{self.channel}': self.is_on
        }