package com.environment.control.device;

import java.time.Instant;
import java.util.List;
import java.util.Optional;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class DeviceService {

    private final DeviceRepository deviceRepository;

    public DeviceService(DeviceRepository deviceRepository) {
        this.deviceRepository = deviceRepository;
    }

    public Optional<Device> findByDeviceId(String deviceId) {
        return deviceRepository.findByDeviceId(deviceId);
    }

    public List<Device> listDevices() {
        return deviceRepository.findAll();
    }

    public Device register(String deviceId, String secret, String name) {
        Device device = new Device();
        device.setDeviceId(deviceId);
        device.setSecret(secret);
        device.setName(name);
        return deviceRepository.save(device);
    }

    @Transactional
    public void touch(Device device) {
        device.setLastSeen(Instant.now());
        deviceRepository.save(device);
    }

    @Transactional
    public void requestUpload(Device device) {
        device.setUploadRequested(true);
        deviceRepository.save(device);
    }

    @Transactional
    public void clearRequest(Device device) {
        device.setUploadRequested(false);
        deviceRepository.save(device);
    }
}
