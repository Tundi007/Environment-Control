package com.environment.control.web;

import com.environment.control.data.DataIngestionService;
import com.environment.control.device.Device;
import com.environment.control.device.DeviceCommunicationService;
import com.environment.control.device.DeviceService;
import java.util.List;
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
}
