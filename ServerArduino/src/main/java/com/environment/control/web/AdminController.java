package com.environment.control.web;

import com.environment.control.data.DataIngestionService;
import com.environment.control.data.DeviceData;
import com.environment.control.device.Device;
import com.environment.control.device.DeviceCommunicationService;
import com.environment.control.device.DeviceService;
import com.environment.control.integration.ChatGptService;
import com.environment.control.web.view.ChartDataPoint;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.servlet.mvc.support.RedirectAttributes;

@Controller
public class AdminController {

    private final DeviceService deviceService;
    private final DataIngestionService dataIngestionService;
    private final DeviceCommunicationService deviceCommunicationService;
    private final ChatGptService chatGptService;
    private final ObjectMapper objectMapper;

    public AdminController(DeviceService deviceService,
                           DataIngestionService dataIngestionService,
                           DeviceCommunicationService deviceCommunicationService,
                           ChatGptService chatGptService,
                           ObjectMapper objectMapper) {
        this.deviceService = deviceService;
        this.dataIngestionService = dataIngestionService;
        this.deviceCommunicationService = deviceCommunicationService;
        this.chatGptService = chatGptService;
        this.objectMapper = objectMapper;
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

    @GetMapping("/admin/devices/{deviceId}/analysis")
    public String analyze(@PathVariable String deviceId, Model model) throws JsonProcessingException {
        Device device = deviceService.findByDeviceId(deviceId).orElse(null);
        if (device == null) {
            return "redirect:/";
        }
        List<DeviceData> records = dataIngestionService.getData(device);
        try {
            String summary = chatGptService.summarizeDevice(device, records);
            model.addAttribute("summary", summary);
            model.addAttribute("error", null);
        } catch (IllegalStateException e) {
            model.addAttribute("error", e.getMessage());
        } catch (Exception e) {
            model.addAttribute("error", "Failed to contact ChatGPT: " + e.getMessage());
        }
        model.addAttribute("recordCount", records.size());
        model.addAttribute("device", device);
        return "analysis";
    }

    @PostMapping("/admin/devices")
    public String register(@RequestParam String deviceId,
                           @RequestParam String secret,
                           @RequestParam String name,
                           @RequestParam(required = false, defaultValue = "") String endpointUrl,
                           @RequestParam(required = false, defaultValue = "") String remoteControlEndpoint) {
        deviceService.register(deviceId, secret, name, endpointUrl, remoteControlEndpoint);
        return "redirect:/?selected=" + deviceId;
    }

    @PostMapping("/admin/devices/{deviceId}/refresh")
    public String refresh(@PathVariable String deviceId) {
        deviceService.findByDeviceId(deviceId).ifPresent(deviceCommunicationService::pullFromDevice);
        return "redirect:/?selected=" + deviceId;
    }

    @GetMapping("/admin/devices/{deviceId}/remote-control")
    public String remoteControl(@PathVariable String deviceId, Model model) {
        Device device = deviceService.findByDeviceId(deviceId).orElse(null);
        if (device == null) {
            return "redirect:/";
        }
        model.addAttribute("device", device);
        return "remote-control";
    }

    @PostMapping("/admin/devices/{deviceId}/remote-control")
    public String sendRemoteControlCommand(@PathVariable String deviceId,
                                           @RequestParam String command,
                                           RedirectAttributes redirectAttributes) {
        Device device = deviceService.findByDeviceId(deviceId).orElse(null);
        if (device == null) {
            return "redirect:/";
        }

        if (!"increase".equalsIgnoreCase(command) && !"decrease".equalsIgnoreCase(command)) {
            redirectAttributes.addFlashAttribute("status", "error");
            redirectAttributes.addFlashAttribute("message", "Unknown remote control command.");
            return "redirect:/admin/devices/" + deviceId + "/remote-control";
        }

        if (device.getRemoteControlEndpoint() == null || device.getRemoteControlEndpoint().isBlank()) {
            redirectAttributes.addFlashAttribute("status", "error");
            redirectAttributes.addFlashAttribute("message", "No remote control endpoint configured for this device.");
            return "redirect:/admin/devices/" + deviceId + "/remote-control";
        }

        String normalizedCommand = command.toLowerCase();
        boolean sent = deviceCommunicationService.sendRemoteControlCommand(device, normalizedCommand);
        redirectAttributes.addFlashAttribute("status", sent ? "success" : "error");
        redirectAttributes.addFlashAttribute(
                "message",
                sent
                        ? "Sent '" + normalizedCommand + "' command to the device."
                        : "Failed to reach the device for the requested command.");
        return "redirect:/admin/devices/" + deviceId + "/remote-control";
    }

    @PostMapping("/admin/devices/{deviceId}/delete")
    public String delete(@PathVariable String deviceId) {
        deviceService.findByDeviceId(deviceId).ifPresent(deviceService::delete);
        return "redirect:/";
    }

    private ChartDataPoint toChartPoint(DeviceData data) {
        Map<String, Object> payload = parsePayload(data.getPayload());
        Double mq135 = toDouble(payload.get("mq135"));
        Double humidity = toDouble(payload.get("humidity"));
        Double temperature = toDouble(payload.get("temperature"));
        Double distance = toDouble(payload.get("distance"));
        if (mq135 == null && humidity == null && temperature == null && distance == null) {
            return null;
        }
        return new ChartDataPoint(data.getCreatedAt(), mq135, humidity, temperature, distance);
    }

    private Map<String, Object> parsePayload(String payloadString) {
        if (payloadString == null || payloadString.isBlank()) {
            return Map.of();
        }

        // Try JSON payload first
        try {
            Map<String, Object> parsed = objectMapper.readValue(
                    payloadString,
                    new TypeReference<Map<String, Object>>() {
                    });
            return normalizeKeys(parsed);
        } catch (Exception ignored) {
            // Fall back to simple comma-separated key=value pairs
        }

        Map<String, Object> parsed = new HashMap<>();
        for (String entry : payloadString.split(",")) {
            String[] parts = entry.split("=", 2);
            if (parts.length != 2) {
                continue;
            }
            parsed.put(parts[0].trim(), parts[1].trim());
        }
        return normalizeKeys(parsed);
    }

    private Map<String, Object> normalizeKeys(Map<String, Object> raw) {
        Map<String, Object> normalized = new HashMap<>();
        copyIfPresent(raw, normalized, "mq135", "mq135");
        copyIfPresent(raw, normalized, "humidity", "humidity");
        copyIfPresent(raw, normalized, "temperature", "temperature");
        copyIfPresent(raw, normalized, "tempC", "temperature");
        copyIfPresent(raw, normalized, "distance", "distance");
        copyIfPresent(raw, normalized, "distanceCm", "distance");
        return normalized;
    }

    private void copyIfPresent(Map<String, Object> source, Map<String, Object> target, String from, String to) {
        if (source.containsKey(from)) {
            target.put(to, source.get(from));
        }
    }

    private Double toDouble(Object value) {
        Double number = null;
        if (value instanceof Number num) {
            number = num.doubleValue();
        } else if (value != null) {
            try {
                number = Double.parseDouble(value.toString());
            } catch (NumberFormatException ignored) {
                return null;
            }
        }
        if (number == null || !Double.isFinite(number)) {
            return null;
        }
        return number;
    }
}
