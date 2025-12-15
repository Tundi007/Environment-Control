package com.environment.control.web;

import com.environment.control.device.DuplicateDeviceException;
import org.springframework.http.HttpStatus;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.ControllerAdvice;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.ResponseStatus;

@ControllerAdvice
public class GlobalExceptionHandler {

    @ResponseStatus(HttpStatus.BAD_REQUEST)
    @ExceptionHandler(DuplicateDeviceException.class)
    public String handleDuplicateDevice(DuplicateDeviceException ex, Model model) {
        model.addAttribute("message", ex.getMessage());
        return "error/duplicate-device";
    }
}
