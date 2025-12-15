package com.environment.control.device;

import java.time.Instant;
import java.util.List;
import java.util.Optional;
import com.environment.control.data.DeviceDataRepository;
import org.springframework.dao.DataIntegrityViolationException;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class DeviceService {

    private final DeviceRepository deviceRepository;
    private final DeviceDataRepository deviceDataRepository;

    public DeviceService(DeviceRepository deviceRepository, DeviceDataRepository deviceDataRepository) {
        this.deviceRepository = deviceRepository;
        this.deviceDataRepository = deviceDataRepository;
    }

    public Optional<Device> findByDeviceId(String deviceId) {
        return deviceRepository.findByDeviceId(deviceId);
    }

    public List<Device> listDevices() {
        return deviceRepository.findAll();
    }

    public Device register(String deviceId,
                          String secret,
                          String name,
                          String endpointUrl,
                          String remoteControlEndpoint) {
        deviceRepository.findByDeviceId(deviceId).ifPresent(existing -> {
            throw new DuplicateDeviceException(deviceId);
        });
        Device device = new Device();
        device.setDeviceId(deviceId);
        device.setSecret(secret);
        device.setName(name);
        device.setEndpointUrl(endpointUrl);
        device.setRemoteControlEndpoint(remoteControlEndpoint);
        try {
            return deviceRepository.save(device);
        } catch (DataIntegrityViolationException ex) {
            throw new DuplicateDeviceException(deviceId, ex);
        }
    }

    @Transactional
    public void touch(Device device) {
        device.setLastSeen(Instant.now());
        deviceRepository.save(device);
    }

    @Transactional
    public void delete(Device device) {
        deviceDataRepository.deleteByDevice(device);
        deviceRepository.delete(device);
    }
}
