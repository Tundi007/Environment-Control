package com.environment.control.web;

import com.environment.control.data.DataIngestionService;
import com.environment.control.device.Device;
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

    public AdminController(DeviceService deviceService,
                           DataIngestionService dataIngestionService) {
        this.deviceService = deviceService;
        this.dataIngestionService = dataIngestionService;
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
    public String register(@RequestParam String deviceId, @RequestParam String secret, @RequestParam String name) {
        deviceService.register(deviceId, secret, name);
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
}
