package com.environment.control.integration;

import com.environment.control.data.DeviceData;
import com.environment.control.device.Device;
import com.fasterxml.jackson.annotation.JsonIgnoreProperties;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.time.Instant;
import java.time.format.DateTimeFormatter;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.stereotype.Service;
import org.springframework.web.reactive.function.client.WebClient;

@Service
public class ChatGptService {

    private static final String MODEL = "gpt-4o-mini";
    private static final int MAX_RECORDS = 20;

    private final String apiKey;
    private final ObjectMapper objectMapper;
    private final WebClient webClient;

    public ChatGptService(@Value("${app.openai.api-key:}") String apiKey,
                          ObjectMapper objectMapper,
                          WebClient.Builder webClientBuilder) {
        this.apiKey = apiKey;
        this.objectMapper = objectMapper;
        this.webClient = webClientBuilder
                .baseUrl("https://api.openai.com/v1")
                .defaultHeader(HttpHeaders.CONTENT_TYPE, MediaType.APPLICATION_JSON_VALUE)
                .build();
    }

    public String summarizeDevice(Device device, List<DeviceData> records) throws JsonProcessingException {
        if (apiKey == null || apiKey.isBlank()) {
            throw new IllegalStateException("OpenAI API key is not configured. Set app.openai.api-key.");
        }

        List<DeviceData> latest = records.stream()
                .sorted(Comparator.comparing(this::timelineInstant).reversed())
                .limit(MAX_RECORDS)
                .sorted(Comparator.comparing(this::timelineInstant))
                .toList();

        String deviceSummary = buildDeviceSummary(device, latest);
        ChatCompletionRequest request = new ChatCompletionRequest(
                MODEL,
                List.of(new ChatMessage("system", "You are an assistant that summarizes IoT telemetry."),
                        new ChatMessage("user", deviceSummary))
        );

        ChatCompletionResponse response = webClient.post()
                .uri("/chat/completions")
                .header(HttpHeaders.AUTHORIZATION, "Bearer " + apiKey)
                .bodyValue(objectMapper.writeValueAsString(request))
                .retrieve()
                .bodyToMono(ChatCompletionResponse.class)
                .block();

        if (response == null || response.getChoices() == null || response.getChoices().isEmpty()) {
            throw new IllegalStateException("No response from ChatGPT.");
        }
        ChatCompletionResponse.Message message = response.getChoices().get(0).getMessage();
        if (message == null || message.getContent() == null) {
            throw new IllegalStateException("ChatGPT returned an empty message.");
        }
        return message.getContent().trim();
    }

    private String buildDeviceSummary(Device device, List<DeviceData> latest) {
        String payloads = latest.isEmpty()
                ? "(No stored records)"
                : latest.stream()
                .map(data -> "#" + data.getSequenceNumber() + " @ "
                        + DateTimeFormatter.ISO_INSTANT.format(timelineInstant(data)) + " => "
                        + normalizePayload(data.getPayload()))
                .collect(Collectors.joining("\n"));

        return "Provide a concise summary of this device's readings and any notable anomalies. "
                + "Use bullet points and end with an overall health verdict.\n\n"
                + "Device: " + device.getName() + " (" + device.getDeviceId() + ")\n"
                + "Endpoint: " + device.getEndpointUrl() + "\n"
                + "Records (newest last, max " + MAX_RECORDS + "):\n" + payloads;
    }

    private String normalizePayload(String payload) {
        try {
            Map<?, ?> map = objectMapper.readValue(payload, Map.class);
            return map.entrySet().stream()
                    .map(entry -> entry.getKey() + ": " + entry.getValue())
                    .collect(Collectors.joining(", "));
        } catch (Exception ignored) {
            return payload;
        }
    }

    private Instant timelineInstant(DeviceData data) {
        return data.getSampledAt() != null ? data.getSampledAt() : data.getCreatedAt();
    }

    private record ChatCompletionRequest(String model, List<ChatMessage> messages) {
    }

    private record ChatMessage(String role, String content) {
    }

    @JsonIgnoreProperties(ignoreUnknown = true)
    private static class ChatCompletionResponse {
        private List<Choice> choices;

        public List<Choice> getChoices() {
            return choices;
        }

        public void setChoices(List<Choice> choices) {
            this.choices = choices;
        }

        @JsonIgnoreProperties(ignoreUnknown = true)
        private static class Choice {
            private Message message;

            public Message getMessage() {
                return message;
            }

            public void setMessage(Message message) {
                this.message = message;
            }
        }

        @JsonIgnoreProperties(ignoreUnknown = true)
        private static class Message {
            private String content;

            public String getContent() {
                return content;
            }

            public void setContent(String content) {
                this.content = content;
            }
        }
    }
}
