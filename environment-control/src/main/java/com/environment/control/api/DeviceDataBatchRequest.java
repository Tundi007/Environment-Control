package com.environment.control.api;

import java.util.ArrayList;
import java.util.List;

public class DeviceDataBatchRequest {
    private List<DeviceDataRecord> records = new ArrayList<>();

    public List<DeviceDataRecord> getRecords() {
        return records;
    }

    public void setRecords(List<DeviceDataRecord> records) {
        this.records = records;
    }
}
