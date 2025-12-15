package com.environment.control.data;

import com.environment.control.device.Device;
import java.util.List;
import org.springframework.data.jpa.repository.JpaRepository;

public interface DeviceDataRepository extends JpaRepository<DeviceData, Long> {
    List<DeviceData> findByDeviceOrderBySequenceNumberAsc(Device device);

    void deleteByDevice(Device device);

    boolean existsByDeviceAndSequenceNumber(Device device, Long sequenceNumber);
}
