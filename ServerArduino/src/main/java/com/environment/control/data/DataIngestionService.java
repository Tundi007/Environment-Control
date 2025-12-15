package com.environment.control.data;

import com.environment.control.device.Device;
import com.environment.control.device.DeviceRepository;
import jakarta.transaction.Transactional;
import java.util.Comparator;
import java.util.List;
import org.springframework.dao.DataIntegrityViolationException;
import org.springframework.stereotype.Service;

@Service
public class DataIngestionService {

    private final DeviceDataRepository deviceDataRepository;
    private final DeviceRepository deviceRepository;

    public DataIngestionService(DeviceDataRepository deviceDataRepository, DeviceRepository deviceRepository) {
        this.deviceDataRepository = deviceDataRepository;
        this.deviceRepository = deviceRepository;
    }

    @Transactional
    public long ingest(Device device, List<DeviceData> records) {
        long maxSequence = device.getLastSequenceAcknowledged() != null ? device.getLastSequenceAcknowledged() : -1;
        for (DeviceData data : records) {
            if (data.getSequenceNumber() != null) {
                if (deviceDataRepository.existsByDeviceAndSequenceNumber(device, data.getSequenceNumber())) {
                    continue;
                }
                maxSequence = Math.max(maxSequence, data.getSequenceNumber());
            }
            data.setDevice(device);
            try {
                deviceDataRepository.save(data);
            } catch (DataIntegrityViolationException ignored) {
                // Duplicate sequence number for this device, skip
            }
        }
        device.setLastSequenceAcknowledged(maxSequence);
        deviceRepository.save(device);
        return maxSequence;
    }

    public List<DeviceData> getData(Device device) {
        return deviceDataRepository.findByDeviceOrderBySequenceNumberAsc(device);
    }

    public long resolveHighestSequence(Device device) {
        return getData(device).stream()
                .map(DeviceData::getSequenceNumber)
                .filter(java.util.Objects::nonNull)
                .max(Comparator.naturalOrder())
                .orElse(device.getLastSequenceAcknowledged() != null ? device.getLastSequenceAcknowledged() : -1L);
    }
}
