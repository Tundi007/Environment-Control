package com.environment.control.config;

import com.environment.control.device.DeviceService;
import org.springframework.boot.CommandLineRunner;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

@Configuration
public class DataInitializer {

    @Bean
    CommandLineRunner seedDevices(DeviceService deviceService) {
        return args -> {
            if (deviceService.listDevices().isEmpty()) {
                deviceService.register("demo-device", "demo-secret", "Demo greenhouse controller");
            }
        };
    }
}
