package com.environment.control.web.view;

import java.time.Instant;

public class ChartDataPoint {

    private final Instant timestamp;
    private final Double mq135;
    private final Double humidity;
    private final Double temperature;
    private final Double distance;

    public ChartDataPoint(Instant timestamp, Double mq135, Double humidity, Double temperature, Double distance) {
        this.timestamp = timestamp;
        this.mq135 = mq135;
        this.humidity = humidity;
        this.temperature = temperature;
        this.distance = distance;
    }

    public Instant getTimestamp() {
        return timestamp;
    }

    public Double getMq135() {
        return mq135;
    }

    public Double getHumidity() {
        return humidity;
    }

    public Double getTemperature() {
        return temperature;
    }

    public Double getDistance() {
        return distance;
    }
}
