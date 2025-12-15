package com.environment.control.api;

public class DeviceDataRecord {
    private Long sequenceNumber;
    private String payload;
    private java.time.Instant sampledAt;

    public Long getSequenceNumber() {
        return sequenceNumber;
    }

    public void setSequenceNumber(Long sequenceNumber) {
        this.sequenceNumber = sequenceNumber;
    }

    public String getPayload() {
        return payload;
    }

    public void setPayload(String payload) {
        this.payload = payload;
    }

    public java.time.Instant getSampledAt() {
        return sampledAt;
    }

    public void setSampledAt(java.time.Instant sampledAt) {
        this.sampledAt = sampledAt;
    }
}
