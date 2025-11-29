package com.environment.control.api;

public class DeviceLoginResponse {
    private String token;
    private String deviceId;

    public DeviceLoginResponse(String token, String deviceId) {
        this.token = token;
        this.deviceId = deviceId;
    }

    public String getToken() {
        return token;
    }

    public String getDeviceId() {
        return deviceId;
    }
}
