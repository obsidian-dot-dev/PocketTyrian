//
// AXI4 Slave Wrapper for SRAM (sram_controller word interface)
//
// Converts AXI4 transactions to the sram_controller word-level protocol.
// SRAM only supports single-word operations (no burst at hardware level),
// so AXI4 bursts are decomposed into sequential single-word accesses.
//
// Protocol: word_rd/wr pulse → busy goes HIGH → wait → rdata_valid (reads) / !busy (writes)
//

`default_nettype none

module axi_sram_slave (
    input wire clk,
    input wire reset_n,

    // AXI4 Slave interface
    // AR channel (read address)
    input  wire        s_axi_arvalid,
    output reg         s_axi_arready,
    input  wire [31:0] s_axi_araddr,
    input  wire [7:0]  s_axi_arlen,

    // R channel (read data)
    output reg         s_axi_rvalid,
    input  wire        s_axi_rready,
    output reg  [31:0] s_axi_rdata,
    output reg  [1:0]  s_axi_rresp,
    output reg         s_axi_rlast,

    // AW channel (write address)
    input  wire        s_axi_awvalid,
    output reg         s_axi_awready,
    input  wire [31:0] s_axi_awaddr,
    input  wire [7:0]  s_axi_awlen,

    // W channel (write data)
    input  wire        s_axi_wvalid,
    output reg         s_axi_wready,
    input  wire [31:0] s_axi_wdata,
    input  wire [3:0]  s_axi_wstrb,
    input  wire        s_axi_wlast,

    // B channel (write response)
    output reg         s_axi_bvalid,
    input  wire        s_axi_bready,
    output reg  [1:0]  s_axi_bresp,

    // SRAM word interface (to sram_controller)
    output reg         sram_rd,
    output reg         sram_wr,
    output reg  [21:0] sram_addr,
    output reg  [31:0] sram_wdata,
    output reg  [3:0]  sram_wstrb,
    input  wire [31:0] sram_rdata,
    input  wire        sram_busy,
    input  wire        sram_rdata_valid
);

wire reset = ~reset_n;

// FSM states
localparam S_IDLE       = 4'd0;
localparam S_RD_CMD     = 4'd1;
localparam S_RD_DAT     = 4'd2;
localparam S_WR_CMD     = 4'd3;
localparam S_WR_WAIT    = 4'd4;
localparam S_WR_NEXT    = 4'd5;

reg [3:0] state;

// Transaction tracking
reg [7:0]  burst_len;
reg [7:0]  beat_count;
reg [31:0] addr_r;
reg        cmd_issued;
reg        sram_started;
reg [7:0]  issue_wait;

wire beat_is_last = (beat_count == burst_len);

always @(posedge clk or posedge reset) begin
    if (reset) begin
        state <= S_IDLE;
        burst_len <= 0;
        beat_count <= 0;
        addr_r <= 0;
        cmd_issued <= 0;
        sram_started <= 0;
        issue_wait <= 0;

        s_axi_arready <= 0;
        s_axi_rvalid <= 0;
        s_axi_rdata <= 0;
        s_axi_rresp <= 0;
        s_axi_rlast <= 0;
        s_axi_awready <= 0;
        s_axi_wready <= 0;
        s_axi_bvalid <= 0;
        s_axi_bresp <= 0;

        sram_rd <= 0;
        sram_wr <= 0;
        sram_addr <= 0;
        sram_wdata <= 0;
        sram_wstrb <= 0;
    end else begin
        // Defaults
        s_axi_arready <= 0;
        s_axi_awready <= 0;
        s_axi_wready <= 0;
        s_axi_rvalid <= 0;
        s_axi_bvalid <= 0;
        sram_rd <= 0;
        sram_wr <= 0;

        case (state)

        S_IDLE: begin
            cmd_issued <= 0;
            sram_started <= 0;
            issue_wait <= 0;
            if (s_axi_arvalid) begin
                s_axi_arready <= 1;
                addr_r <= s_axi_araddr;
                burst_len <= s_axi_arlen;
                beat_count <= 0;
                state <= S_RD_CMD;
            end else if (s_axi_awvalid) begin
                s_axi_awready <= 1;
                addr_r <= s_axi_awaddr;
                burst_len <= s_axi_awlen;
                beat_count <= 0;
                if (s_axi_wvalid) begin
                    s_axi_wready <= 1;
                    sram_wdata <= s_axi_wdata;
                    sram_wstrb <= s_axi_wstrb;
                    state <= S_WR_CMD;
                end else begin
                    state <= S_WR_NEXT;
                end
            end
        end

        S_RD_CMD: begin
            if (!cmd_issued) begin
                if (!sram_busy) begin
                    sram_rd <= 1;
                    sram_addr <= addr_r[23:2];
                    cmd_issued <= 1;
                    sram_started <= 0;
                    issue_wait <= 0;
                end
            end else begin
                if (!sram_started) begin
                    if (sram_busy) begin
                        sram_started <= 1;
                        issue_wait <= 0;
                    end else begin
                        issue_wait <= issue_wait + 1;
                        if (&issue_wait) begin
                            cmd_issued <= 0;
                            issue_wait <= 0;
                        end
                    end
                end
                if (sram_rdata_valid) begin
                    state <= S_RD_DAT;
                end
            end
        end

        S_RD_DAT: begin
            s_axi_rvalid <= 1;
            s_axi_rdata <= sram_rdata;
            s_axi_rresp <= 2'b00;
            s_axi_rlast <= beat_is_last;
            beat_count <= beat_count + 1;
            cmd_issued <= 0;
            sram_started <= 0;
            if (beat_is_last) begin
                state <= S_IDLE;
            end else begin
                addr_r <= addr_r + 32'd4;
                state <= S_RD_CMD;
            end
        end

        S_WR_CMD: begin
            if (!cmd_issued) begin
                if (!sram_busy) begin
                    sram_wr <= 1;
                    sram_addr <= addr_r[23:2];
                    cmd_issued <= 1;
                    sram_started <= 0;
                    issue_wait <= 0;
                end
            end else begin
                if (!sram_started && sram_busy) begin
                    sram_started <= 1;
                    issue_wait <= 0;
                end else if (!sram_started) begin
                    issue_wait <= issue_wait + 1;
                    if (&issue_wait) begin
                        cmd_issued <= 0;
                        issue_wait <= 0;
                    end
                end else if (sram_started && !sram_busy) begin
                    state <= S_WR_WAIT;
                end
            end
        end

        S_WR_WAIT: begin
            beat_count <= beat_count + 1;
            cmd_issued <= 0;
            sram_started <= 0;
            if (beat_is_last) begin
                s_axi_bvalid <= 1;
                s_axi_bresp <= 2'b00;
                state <= S_IDLE;
            end else begin
                addr_r <= addr_r + 32'd4;
                state <= S_WR_NEXT;
            end
        end

        S_WR_NEXT: begin
            if (s_axi_wvalid) begin
                s_axi_wready <= 1;
                sram_wdata <= s_axi_wdata;
                sram_wstrb <= s_axi_wstrb;
                state <= S_WR_CMD;
            end
        end

        default: state <= S_IDLE;

        endcase
    end
end

endmodule
