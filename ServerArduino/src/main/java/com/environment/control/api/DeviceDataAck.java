package com.environment.control.api;

public class DeviceDataAck {
    private long lastProcessedSequence;

    public DeviceDataAck(long lastProcessedSequence) {
        this.lastProcessedSequence = lastProcessedSequence;
    }

    public long getLastProcessedSequence() {
        return lastProcessedSequence;
    }
}
