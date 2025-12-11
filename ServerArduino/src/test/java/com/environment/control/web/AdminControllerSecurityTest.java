package com.environment.control.web;

import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.WebMvcTest;
import org.springframework.boot.test.mock.mockito.MockBean;
import org.springframework.context.annotation.Import;
import org.springframework.test.web.servlet.MockMvc;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.redirectedUrl;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import com.environment.control.data.DataIngestionService;
import com.environment.control.device.DeviceCommunicationService;
import com.environment.control.device.DeviceService;
import com.environment.control.security.DeviceAuthenticationFilter;
import com.environment.control.security.SecurityConfig;

@WebMvcTest(controllers = AdminController.class)
@Import(SecurityConfig.class)
class AdminControllerSecurityTest {

    @Autowired
    private MockMvc mockMvc;

    @MockBean
    private DeviceService deviceService;

    @MockBean
    private DataIngestionService dataIngestionService;

    @MockBean
    private DeviceCommunicationService deviceCommunicationService;

    @MockBean
    private DeviceAuthenticationFilter deviceAuthenticationFilter;

    @Test
    void refreshIsAccessibleWithoutAuthentication() throws Exception {
        mockMvc.perform(post("/admin/devices/device-123/refresh"))
                .andExpect(status().is3xxRedirection())
                .andExpect(redirectedUrl("/?selected=device-123"));
    }
}