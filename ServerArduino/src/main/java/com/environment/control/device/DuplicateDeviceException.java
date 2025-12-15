package com.environment.control.device;

public class DuplicateDeviceException extends RuntimeException {

    public DuplicateDeviceException(String deviceId) {
        super("Device with ID '" + deviceId + "' already exists");
    }

    public DuplicateDeviceException(String deviceId, Throwable cause) {
        super("Device with ID '" + deviceId + "' already exists", cause);
    }
}
