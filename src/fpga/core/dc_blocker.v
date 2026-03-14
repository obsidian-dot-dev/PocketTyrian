//
// DC blocker filter using a simplified IIR high-pass structure.
//
// Removes DC offset from audio signals to eliminate clicks/pops when
// OPL2 voices key on/off or when mixing levels change abruptly.
//
// Based on dc_blocker.sv by Alexey Melnikov / OpenGateware (GPL-3.0).
// Converted to plain Verilog for PocketDoom.
//
// Transfer function (z-domain):
//   y[n] = x[n] - x[n-1] + alpha * y[n-1]
// where alpha ≈ 1 - 2^(-shift), giving a very low corner frequency
// (~23 Hz at 48 kHz with shift=10).
//

`default_nettype none

module dc_blocker (
    input  wire        clk,
    input  wire        ce,           // Clock enable at sample rate (48 kHz)
    input  wire [15:0] din,          // 16-bit signed input
    output wire [15:0] dout          // 16-bit signed output
);

// Sign-extend input to 40-bit fixed-point (16.23 format)
wire [39:0] x  = {din[15], din, 23'd0};

// High-pass: subtract a fraction of input (sets corner frequency)
// Shift by 10 → corner freq ≈ 48000 / (2π × 2^10) ≈ 7.5 Hz
wire [39:0] x0 = x - {{10{x[39]}}, x[39:10]};

// Feedback: subtract a fraction of previous output (decay rate)
// Shift by 9 → alpha ≈ 1 - 2^(-9) ≈ 0.998
wire [39:0] y1 = y - {{9{y[39]}}, y[39:9]};

// IIR output: y[n] = x0[n] - x1[n-1] + y1[n-1]
wire [39:0] y0 = x0 - x1 + y1;

// State registers
reg [39:0] x1 = 40'd0;
reg [39:0] y  = 40'd0;

always @(posedge clk) begin
    if (ce) begin
        x1 <= x0;
        // Clamp on overflow (if bits 39 and 38 disagree)
        y  <= ^y0[39:38] ? {{2{y0[39]}}, {38{y0[38]}}} : y0;
    end
end

// Extract 16-bit output from fixed-point accumulator
assign dout = y[38:23];

endmodule
