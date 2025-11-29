package com.environment.control.security;

import com.environment.control.device.Device;
import com.environment.control.device.DeviceRepository;
import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.util.Optional;
import org.springframework.http.HttpHeaders;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.context.SecurityContextHolder;
import org.springframework.security.core.userdetails.User;
import org.springframework.security.web.authentication.WebAuthenticationDetailsSource;
import org.springframework.stereotype.Component;
import org.springframework.web.filter.OncePerRequestFilter;

@Component
public class DeviceAuthenticationFilter extends OncePerRequestFilter {

    private final TokenService tokenService;
    private final DeviceRepository deviceRepository;

    public DeviceAuthenticationFilter(TokenService tokenService, DeviceRepository deviceRepository) {
        this.tokenService = tokenService;
        this.deviceRepository = deviceRepository;
    }

    @Override
    protected void doFilterInternal(HttpServletRequest request, HttpServletResponse response, FilterChain filterChain)
            throws ServletException, IOException {
        String header = request.getHeader(HttpHeaders.AUTHORIZATION);
        if (header != null && header.startsWith("Bearer ")) {
            String token = header.substring(7);
            try {
                String deviceId = tokenService.parseDeviceId(token);
                Optional<Device> device = deviceRepository.findByDeviceId(deviceId);
                if (device.isPresent()) {
                    User principal = new User(device.get().getDeviceId(), "N/A", java.util.List.of());
                    Authentication authentication = new UsernamePasswordAuthenticationToken(principal, null, principal.getAuthorities());
                    ((UsernamePasswordAuthenticationToken) authentication).setDetails(
                            new WebAuthenticationDetailsSource().buildDetails(request));
                    SecurityContextHolder.getContext().setAuthentication(authentication);
                }
            } catch (Exception ignored) {
                SecurityContextHolder.clearContext();
            }
        }
        filterChain.doFilter(request, response);
    }
}
