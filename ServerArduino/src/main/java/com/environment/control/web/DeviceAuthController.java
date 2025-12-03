package com.environment.control.web;

import com.environment.control.api.DeviceLoginRequest;
import com.environment.control.api.DeviceLoginResponse;
import com.environment.control.device.Device;
import com.environment.control.device.DeviceService;
import com.environment.control.security.TokenService;
import java.time.Instant;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/devices")
public class DeviceAuthController {

    private final DeviceService deviceService;
    private final TokenService tokenService;

    public DeviceAuthController(DeviceService deviceService, TokenService tokenService) {
        this.deviceService = deviceService;
        this.tokenService = tokenService;
    }

    @PostMapping("/login")
    public ResponseEntity<DeviceLoginResponse> login(@RequestBody DeviceLoginRequest request) {
        return deviceService.findByDeviceId(request.getDeviceId())
                .filter(device -> device.getSecret().equals(request.getSecret()))
                .map(device -> {
                    device.setLastSeen(Instant.now());
                    deviceService.touch(device);
                    return ResponseEntity.ok(new DeviceLoginResponse(tokenService.generateToken(device.getDeviceId()), device.getDeviceId()));
                })
                .orElse(ResponseEntity.status(401).build());
    }
}
