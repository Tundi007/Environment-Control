package com.environment.control.web;

import com.environment.control.data.DataIngestionService;
import com.environment.control.data.DeviceData;
import com.environment.control.device.Device;
import com.environment.control.device.DeviceCommunicationService;
import com.environment.control.device.DeviceService;
import com.environment.control.web.view.ChartDataPoint;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestParam;

@Controller
public class AdminController {

    private final DeviceService deviceService;
    private final DataIngestionService dataIngestionService;
    private final DeviceCommunicationService deviceCommunicationService;
    private final ObjectMapper objectMapper = new ObjectMapper();

    public AdminController(DeviceService deviceService,
                           DataIngestionService dataIngestionService,
                           DeviceCommunicationService deviceCommunicationService) {
        this.deviceService = deviceService;
        this.dataIngestionService = dataIngestionService;
        this.deviceCommunicationService = deviceCommunicationService;
    }

    @GetMapping({"/", "/admin"})
    public String dashboard(Model model, @RequestParam(value = "selected", required = false) String selected) {
        List<Device> devices = deviceService.listDevices();
        model.addAttribute("devices", devices);
        if (selected != null) {
            deviceService.findByDeviceId(selected).ifPresent(device -> {
                model.addAttribute("selectedDevice", device);
                model.addAttribute("data", dataIngestionService.getData(device));
            });
        }
        return "index";
    }

    @GetMapping("/admin/devices/{deviceId}/charts")
    public String charts(@PathVariable String deviceId, Model model) throws JsonProcessingException {
        Device device = deviceService.findByDeviceId(deviceId).orElse(null);
        if (device == null) {
            return "redirect:/";
        }
        List<DeviceData> records = dataIngestionService.getData(device);
        List<ChartDataPoint> points = records.stream()
                .map(this::toChartPoint)
                .filter(Objects::nonNull)
                .toList();

        model.addAttribute("device", device);
        model.addAttribute("chartDataJson", points.isEmpty() ? "" : objectMapper.writeValueAsString(points));
        return "charts";
    }

    @PostMapping("/admin/devices")
    public String register(@RequestParam String deviceId,
                           @RequestParam String secret,
                           @RequestParam String name,
                           @RequestParam(required = false, defaultValue = "") String endpointUrl) {
        deviceService.register(deviceId, secret, name, endpointUrl);
        return "redirect:/?selected=" + deviceId;
    }

    @PostMapping("/admin/devices/{deviceId}/request-upload")
    public String requestUpload(@PathVariable String deviceId) {
        deviceService.findByDeviceId(deviceId).ifPresent(deviceService::requestUpload);
        return "redirect:/?selected=" + deviceId;
    }

    @PostMapping("/admin/devices/{deviceId}/clear-upload")
    public String clearUpload(@PathVariable String deviceId) {
        deviceService.findByDeviceId(deviceId).ifPresent(deviceService::clearRequest);
        return "redirect:/?selected=" + deviceId;
    }

    @PostMapping("/admin/devices/{deviceId}/refresh")
    public String refresh(@PathVariable String deviceId) {
        deviceService.findByDeviceId(deviceId).ifPresent(deviceCommunicationService::pullFromDevice);
        return "redirect:/?selected=" + deviceId;
    }

    @PostMapping("/admin/devices/{deviceId}/delete")
    public String delete(@PathVariable String deviceId) {
        deviceService.findByDeviceId(deviceId).ifPresent(deviceService::delete);
        return "redirect:/";
    }

    private ChartDataPoint toChartPoint(DeviceData data) {
        try {
            Map<String, Object> payload = objectMapper.readValue(data.getPayload(), new TypeReference<Map<String, Object>>() {
            });
            Double mq135 = toDouble(payload.get("mq135"));
            Double humidity = toDouble(payload.get("humidity"));
            Double temperature = toDouble(payload.get("temperature"));
            Double distance = toDouble(payload.get("distance"));
            if (mq135 == null && humidity == null && temperature == null && distance == null) {
                return null;
            }
            return new ChartDataPoint(data.getCreatedAt(), mq135, humidity, temperature, distance);
        } catch (Exception e) {
            return null;
        }
    }

    private Double toDouble(Object value) {
        if (value instanceof Number number) {
            return number.doubleValue();
        }
        if (value != null) {
            try {
                return Double.parseDouble(value.toString());
            } catch (NumberFormatException ignored) {
                return null;
            }
        }
        return null;
    }
}
