package com.environment.control.web;

import com.environment.control.api.DeviceDataAck;
import com.environment.control.api.DeviceDataBatchRequest;
import com.environment.control.api.DeviceDataRecord;
import com.environment.control.api.PendingResponse;
import com.environment.control.data.DataIngestionService;
import com.environment.control.data.DeviceData;
import com.environment.control.device.Device;
import com.environment.control.device.DeviceRepository;
import com.environment.control.device.DeviceService;
import java.util.List;
import java.util.stream.Collectors;
import org.springframework.http.ResponseEntity;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.userdetails.User;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.server.ResponseStatusException;
import org.springframework.http.HttpStatus;

@RestController
@RequestMapping("/api/devices")
public class DeviceDataController {

    private final DeviceRepository deviceRepository;
    private final DeviceService deviceService;
    private final DataIngestionService dataIngestionService;

    public DeviceDataController(DeviceRepository deviceRepository, DeviceService deviceService,
                                DataIngestionService dataIngestionService) {
        this.deviceRepository = deviceRepository;
        this.deviceService = deviceService;
        this.dataIngestionService = dataIngestionService;
    }

    @GetMapping("/pending-requests")
    public ResponseEntity<PendingResponse> pending(@RequestParam(name = "longPoll", defaultValue = "false") boolean longPoll,
                                                   @RequestParam(name = "acknowledge", defaultValue = "false") boolean acknowledge,
                                                   Authentication authentication) throws InterruptedException {
        Device device = resolveDevice(authentication);
        if (longPoll && !device.isUploadRequested()) {
            for (int i = 0; i < 20; i++) {
                Thread.sleep(1000);
                device = deviceRepository.findById(device.getId()).orElse(device);
                if (device.isUploadRequested()) {
                    break;
                }
            }
        }
        boolean requested = device.isUploadRequested();
        if (requested && acknowledge) {
            device.setUploadRequested(false);
            deviceRepository.save(device);
        }
        deviceService.touch(device);
        return ResponseEntity.ok(new PendingResponse(requested, device.getLastSequenceAcknowledged()));
    }

    @PostMapping("/data")
    public ResponseEntity<DeviceDataAck> ingest(@RequestBody DeviceDataBatchRequest request, Authentication authentication) {
        Device device = resolveDevice(authentication);
        deviceService.touch(device);
        List<DeviceData> records = request.getRecords().stream()
                .map(this::toEntity)
                .collect(Collectors.toList());
        long last = dataIngestionService.ingest(device, records);
        device.setUploadRequested(false);
        deviceRepository.save(device);
        return ResponseEntity.ok(new DeviceDataAck(last));
    }

    private Device resolveDevice(Authentication authentication) {
        if (authentication == null || !(authentication.getPrincipal() instanceof User user)) {
            throw new ResponseStatusException(HttpStatus.UNAUTHORIZED, "Missing device token");
        }
        return deviceService.findByDeviceId(user.getUsername())
                .orElseThrow(() -> new ResponseStatusException(HttpStatus.UNAUTHORIZED, "Device not found"));
    }

    private DeviceData toEntity(DeviceDataRecord record) {
        DeviceData entity = new DeviceData();
        entity.setSequenceNumber(record.getSequenceNumber());
        entity.setPayload(record.getPayload());
        return entity;
    }
}
