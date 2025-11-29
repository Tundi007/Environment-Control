package com.environment.control.device;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.Table;
import java.time.Instant;

@Entity
@Table(name = "devices")
public class Device {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(unique = true, nullable = false)
    private String deviceId;

    private String name;

    @Column(nullable = false)
    private String secret;

    @Column(nullable = false)
    private boolean uploadRequested = false;

    private Long lastSequenceAcknowledged;

    private Instant lastSeen;

    public Long getId() {
        return id;
    }

    public String getDeviceId() {
        return deviceId;
    }

    public void setDeviceId(String deviceId) {
        this.deviceId = deviceId;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getSecret() {
        return secret;
    }

    public void setSecret(String secret) {
        this.secret = secret;
    }

    public boolean isUploadRequested() {
        return uploadRequested;
    }

    public void setUploadRequested(boolean uploadRequested) {
        this.uploadRequested = uploadRequested;
    }

    public Long getLastSequenceAcknowledged() {
        return lastSequenceAcknowledged;
    }

    public void setLastSequenceAcknowledged(Long lastSequenceAcknowledged) {
        this.lastSequenceAcknowledged = lastSequenceAcknowledged;
    }

    public Instant getLastSeen() {
        return lastSeen;
    }

    public void setLastSeen(Instant lastSeen) {
        this.lastSeen = lastSeen;
    }
}
