-- Create the database if it doesn't exist
CREATE DATABASE IF NOT EXISTS environment_data2;

-- Use the database
USE environment_data2;

-- Create sensor_data table
CREATE TABLE IF NOT EXISTS sensor_data (
    id INT AUTO_INCREMENT PRIMARY KEY,
    temperature FLOAT NOT NULL,
    humidity FLOAT NOT NULL,
    pressure FLOAT NOT NULL,
    altitude FLOAT NOT NULL,
    timestamp DATETIME NOT NULL,
    CONSTRAINT chk_temperature CHECK (temperature >= 0 AND temperature <= 50),
    CONSTRAINT chk_humidity CHECK (humidity >= 0 AND humidity <= 100),
    CONSTRAINT chk_pressure CHECK (pressure >= 900 AND pressure <= 1100),
    CONSTRAINT chk_altitude CHECK (altitude >= 0 AND altitude <= 100),
    INDEX idx_timestamp (timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Create indexes for better query performance
CREATE INDEX idx_temperature ON sensor_data(temperature);
CREATE INDEX idx_humidity ON sensor_data(humidity);
CREATE INDEX idx_pressure ON sensor_data(pressure);
CREATE INDEX idx_altitude ON sensor_data(altitude);
