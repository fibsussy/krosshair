#version 450

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D mask_sampler;
layout(binding = 1) uniform sampler2D game_fb_sampler;

// Push constants: 76 bytes total.  Fixed processing order.
layout(push_constant) uniform PushConstants {
    vec2 quad_ndc_min;       // NDC min corner of the mask quad
    vec2 quad_ndc_size;      // NDC size of the mask quad
    float invert_str;        // Invert strength (0 = disabled)
    float dodge_str;         // Dodge strength
    float dodge_r;           // Dodge tint R
    float dodge_g;           // Dodge tint G
    float dodge_b;           // Dodge tint B
    float burn_str;          // Burn strength
    float burn_r;            // Burn tint R
    float burn_g;            // Burn tint G
    float burn_b;            // Burn tint B
    float complement_str;    // Complement strength
    float lumainvert_str;    // Luma Invert strength
    float huerotate_str;     // Hue Rotate strength
    float huerotate_angle;   // Hue Rotate angle (degrees)
    float saturate_str;      // Saturate strength
    float saturate_amount;   // Saturate amount (0-1)
} pc;

// ── RGB <-> HSL conversions ──────────────────────────────────────

vec3 rgb_to_hsl(vec3 c) {
    float mx = max(c.r, max(c.g, c.b));
    float mn = min(c.r, min(c.g, c.b));
    float l = (mx + mn) * 0.5;
    float d = mx - mn;

    if (d < 0.00001) return vec3(0.0, 0.0, l);

    float s = (l > 0.5) ? d / (2.0 - mx - mn) : d / (mx + mn);
    float h;

    if (mx == c.r)
        h = mod((c.g - c.b) / d, 6.0);
    else if (mx == c.g)
        h = (c.b - c.r) / d + 2.0;
    else
        h = (c.r - c.g) / d + 4.0;

    h /= 6.0;
    if (h < 0.0) h += 1.0;

    return vec3(h, s, l);
}

float hue_to_rgb(float p, float q, float t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0/2.0) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}

vec3 hsl_to_rgb(vec3 hsl) {
    float h = hsl.x, s = hsl.y, l = hsl.z;
    if (s < 0.00001) return vec3(l);
    float q = (l < 0.5) ? l * (1.0 + s) : l + s - l * s;
    float p = 2.0 * l - q;
    return vec3(
        hue_to_rgb(p, q, h + 1.0/3.0),
        hue_to_rgb(p, q, h),
        hue_to_rgb(p, q, h - 1.0/3.0)
    );
}

void main() {
    // Binary mask: check red channel (grayscale)
    float mask = texture(mask_sampler, frag_tex_coord).r;
    if (mask < 0.5) discard;

    // Sample game framebuffer
    vec2 ndc = pc.quad_ndc_min + frag_tex_coord * pc.quad_ndc_size;
    vec2 screen_uv = ndc * 0.5 + 0.5;
    vec3 color = texture(game_fb_sampler, screen_uv).rgb;

    // Fixed order: invert → dodge → burn → complement → luma_invert → hue_rotate → saturate

    if (pc.invert_str > 0.0) {
        color = mix(color, vec3(1.0) - color, pc.invert_str);
    }

    if (pc.dodge_str > 0.0) {
        vec3 dodged = min(color + vec3(pc.dodge_r, pc.dodge_g, pc.dodge_b), vec3(1.0));
        color = mix(color, dodged, pc.dodge_str);
    }

    if (pc.burn_str > 0.0) {
        vec3 burned = max(color - vec3(pc.burn_r, pc.burn_g, pc.burn_b), vec3(0.0));
        color = mix(color, burned, pc.burn_str);
    }

    if (pc.complement_str > 0.0) {
        vec3 hsl = rgb_to_hsl(color);
        hsl.x = mod(hsl.x + 0.5, 1.0);
        hsl.y = 1.0;
        hsl.z = 1.0 - hsl.z;
        color = mix(color, hsl_to_rgb(hsl), pc.complement_str);
    }

    if (pc.lumainvert_str > 0.0) {
        vec3 hsl = rgb_to_hsl(color);
        hsl.z = 1.0 - hsl.z;
        color = mix(color, hsl_to_rgb(hsl), pc.lumainvert_str);
    }

    if (pc.huerotate_str > 0.0) {
        vec3 hsl = rgb_to_hsl(color);
        hsl.x = mod(hsl.x + pc.huerotate_angle / 360.0, 1.0);
        hsl.z = 1.0 - hsl.z;
        color = mix(color, hsl_to_rgb(hsl), pc.huerotate_str);
    }

    if (pc.saturate_str > 0.0) {
        vec3 hsl = rgb_to_hsl(color);
        hsl.y = pc.saturate_amount;
        hsl.z = 1.0 - hsl.z;
        color = mix(color, hsl_to_rgb(hsl), pc.saturate_str);
    }

    out_color = vec4(color, 1.0);
}
