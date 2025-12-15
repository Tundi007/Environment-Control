package com.environment.control.device;

import com.environment.control.api.DeviceDataRecord;
import com.environment.control.data.DataIngestionService;
import com.environment.control.data.DeviceData;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;
import org.springframework.boot.web.client.RestTemplateBuilder;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClientException;
import org.springframework.web.client.RestTemplate;

@Service
public class DeviceCommunicationService {

    private final RestTemplate restTemplate;
    private final DataIngestionService dataIngestionService;
    private final DeviceService deviceService;

    public DeviceCommunicationService(RestTemplateBuilder restTemplateBuilder,
                                      DataIngestionService dataIngestionService,
                                      DeviceService deviceService) {
        this.restTemplate = restTemplateBuilder.build();
        this.dataIngestionService = dataIngestionService;
        this.deviceService = deviceService;
    }

    public int pullFromDevice(Device device) {
        if (device.getEndpointUrl() == null || device.getEndpointUrl().isBlank()) {
            return 0;
        }
        try {
            DeviceDataRecord[] payload = restTemplate.getForObject(device.getEndpointUrl(), DeviceDataRecord[].class);
            List<DeviceDataRecord> records = payload != null
                    ? Arrays.asList(payload)
                    : Collections.emptyList();
            List<DeviceData> entities = records.stream()
                    .filter(record -> record.getSequenceNumber() != null && record.getPayload() != null)
                    .map(record -> toEntity(record, device))
                    .collect(Collectors.toList());
            if (!entities.isEmpty()) {
                dataIngestionService.ingest(device, entities);
                deviceService.touch(device);
            }
            return entities.size();
        } catch (RestClientException ex) {
            return 0;
        }
    }

    private DeviceData toEntity(DeviceDataRecord record, Device device) {
        DeviceData data = new DeviceData();
        data.setDevice(device);
        data.setSequenceNumber(record.getSequenceNumber());
        data.setPayload(record.getPayload());
        return data;
    }

    public boolean sendLedCommand(Device device, String command) {
        if (device.getLedControlEndpoint() == null || device.getLedControlEndpoint().isBlank()) {
            return false;
        }
        try {
            restTemplate.postForEntity(device.getLedControlEndpoint(), Map.of("command", command), Void.class);
            return true;
        } catch (RestClientException ex) {
            return false;
        }
    }
}
