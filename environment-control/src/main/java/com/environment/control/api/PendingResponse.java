package com.environment.control.api;

public class PendingResponse {
    private boolean uploadRequested;
    private Long lastSequenceAcknowledged;

    public PendingResponse(boolean uploadRequested, Long lastSequenceAcknowledged) {
        this.uploadRequested = uploadRequested;
        this.lastSequenceAcknowledged = lastSequenceAcknowledged;
    }

    public boolean isUploadRequested() {
        return uploadRequested;
    }

    public Long getLastSequenceAcknowledged() {
        return lastSequenceAcknowledged;
    }
}
