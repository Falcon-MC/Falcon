package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"github.com/go-gl/mathgl/mgl32"
	"github.com/sandertv/gophertunnel/minecraft"
	"github.com/sandertv/gophertunnel/minecraft/protocol"
	"github.com/sandertv/gophertunnel/minecraft/protocol/login"
	"github.com/sandertv/gophertunnel/minecraft/protocol/packet"

	"github.com/df-mc/go-nethernet"
	"github.com/pion/webrtc/v4"
	"log/slog"
	"net/http"
	_ "net/http/pprof"
	"os"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

type Stats struct {
	Name                                                                 string
	Joined                                                               bool
	Error                                                                string
	Moves, Chats, Adds, Blocks, Hurts, SentMoves, SentChats, SentAttacks int64
	ChatDelayTotalMs, ChatDelayMaxMs                                     int64
}
type Bot struct {
	mu    sync.Mutex
	stats Stats
	conn  *minecraft.Conn
}

var slots = make(chan struct{}, 8)
var groupSize = 50
var active atomic.Bool
var quit = make(chan struct{})

func run(b *Bot, index int, address string, joined chan<- bool) {
	dialer := minecraft.Dialer{IdentityData: login.IdentityData{DisplayName: b.stats.Name, XUID: fmt.Sprint(800000 + index)}, KeepXBLIdentityData: true, MaxDecompressedLen: 256 << 20}

	slots <- struct{}{}
	var c *minecraft.Conn
	var err error
	for attempt := 0; attempt < 4; attempt++ {
		c, err = dialer.DialTimeout("nethernet", address, 60*time.Second)
		if err == nil {
			err = c.DoSpawn()
		}
		if err == nil {
			break
		}
		if c != nil {
			c.Close()
		}
		fmt.Println("RETRY", b.stats.Name, attempt, err)
		time.Sleep(2 * time.Second)
	}
	<-slots
	if err != nil {
		b.mu.Lock()
		b.stats.Error = err.Error()
		b.mu.Unlock()
		joined <- false
		fmt.Println("JOIN_FAILED", b.stats.Name, err)
		return
	}
	defer c.Close()
	b.mu.Lock()
	b.conn = c
	b.stats.Joined = true
	b.mu.Unlock()
	joined <- true
	fmt.Println("JOINED", b.stats.Name)
	c.WritePacket(&packet.RequestChunkRadius{ChunkRadius: 4, MaxChunkRadius: 4})
	own := c.GameData().EntityRuntimeID
	pos := c.GameData().PlayerPosition
	var mu sync.Mutex
	others := map[uint64]mgl32.Vec3{}
	done := make(chan struct{})
	defer close(done)
	go func() {
		timer := time.NewTicker(50 * time.Millisecond)
		defer timer.Stop()
		var tick uint64
		actionTick := uint64(time.Now().UnixMilli()/50) + uint64(index)
		for {
			select {
			case <-done:
				return
			case <-quit:
				return
			case <-timer.C:
				tick++
				actionTick++
				mu.Lock()
				delta := mgl32.Vec3{}
				flags := protocol.NewInputFlags(packet.InputFlagCount)
				if active.Load() {
					direction := float32(1)
					if (tick/20)%2 == 1 {
						direction = -1
					}
					delta[2] = .08 * direction
					pos = pos.Add(delta)
					flags.Set(packet.InputFlagUp)
				}
				pk := &packet.PlayerAuthInput{Position: pos, Delta: delta, Tick: tick, InputMode: packet.InputModeMouse, PlayMode: packet.PlayModeNormal, InteractionModel: packet.InteractionModelCrosshair, InputData: flags, Yaw: 180, HeadYaw: 180, Pitch: 45}
				if active.Load() {
					phase := actionTick % 40
					if phase >= 20 && phase <= 30 {
						pk.Yaw = -90
						if index%2 == 1 {
							pk.Yaw = 90
						}
						pk.HeadYaw = pk.Yaw
						pk.Pitch = 0
					}
					pk.MoveVector = mgl32.Vec2{0, 1}
					pk.RawMoveVector = pk.MoveVector
					if actionTick%40 == 5 {
						target := protocol.BlockPos{int32((index/groupSize%5)*256 + 4 + ((index%groupSize/2)%5)*6 + (index%2)*2), 200, int32((index/groupSize/5)*256 + 3 + (index%groupSize/2)/5*4)}
						pk.InputData.Set(packet.InputFlagPerformBlockActions)
						pk.BlockActions = protocol.Option([]protocol.PlayerBlockAction{{Action: protocol.PlayerActionStartBreak, BlockPos: target, Face: 1}, {Action: protocol.PlayerActionPredictDestroyBlock, BlockPos: target, Face: 1}})
					}
					if actionTick%40 == 25 {
						for id, p := range others {
							if p.Sub(pos).Len() < 3 {
								c.WritePacket(&packet.InventoryTransaction{TransactionData: &protocol.UseItemOnEntityTransactionData{TargetEntityRuntimeID: id, ActionType: protocol.UseItemOnEntityActionAttack, Position: pos}})
								b.mu.Lock()
								b.stats.SentAttacks++
								b.mu.Unlock()
								break
							}
						}
					}
					if actionTick%120 == 32 {
						c.WritePacket(&packet.Text{TextType: packet.TextTypeChat, SourceName: b.stats.Name, Message: fmt.Sprintf("netmix-%s-%d", b.stats.Name, time.Now().UnixMilli()), XUID: fmt.Sprint(800000 + index)})
						b.mu.Lock()
						b.stats.SentChats++
						b.mu.Unlock()
					}
				}
				mu.Unlock()
				if c.WritePacket(pk) != nil {
					return
				}
				if active.Load() {
					b.mu.Lock()
					b.stats.SentMoves++
					b.mu.Unlock()
				}
			}
		}
	}()
	for {
		pk, err := c.ReadPacket()
		if err != nil {
			b.mu.Lock()
			b.stats.Error = err.Error()
			b.mu.Unlock()
			return
		}
		mu.Lock()
		switch p := pk.(type) {
		case *packet.NetworkStackLatency:
			if p.NeedsResponse {
				c.WritePacket(&packet.NetworkStackLatency{Timestamp: p.Timestamp})
			}
		case *packet.CorrectPlayerMovePrediction:
			pos = p.Position
		case *packet.MovePlayer:
			if p.EntityRuntimeID == own {
				pos = p.Position
			} else {
				others[p.EntityRuntimeID] = p.Position
				if active.Load() {
					b.mu.Lock()
					b.stats.Moves++
					b.mu.Unlock()
				}
			}
		case *packet.AddPlayer:
			others[p.EntityRuntimeID] = p.Position
			b.mu.Lock()
			b.stats.Adds++
			b.mu.Unlock()
		case *packet.Text:
			if active.Load() && strings.HasPrefix(p.Message, "netmix-") && p.SourceName != b.stats.Name {
				b.mu.Lock()
				b.stats.Chats++
				if sent, err := strconv.ParseInt(p.Message[strings.LastIndex(p.Message, "-")+1:], 10, 64); err == nil {
					delay := time.Now().UnixMilli() - sent
					b.stats.ChatDelayTotalMs += delay
					if delay > b.stats.ChatDelayMaxMs {
						b.stats.ChatDelayMaxMs = delay
					}
				}
				b.mu.Unlock()
			}
		case *packet.UpdateBlock:
			if active.Load() {
				b.mu.Lock()
				b.stats.Blocks++
				b.mu.Unlock()
			}
		}
		mu.Unlock()
	}
}
func main() {

	var settings webrtc.SettingEngine
	settings.SetNetworkTypes([]webrtc.NetworkType{webrtc.NetworkTypeUDP4})
	settings.SetIncludeLoopbackCandidate(true)
	settings.SetInterfaceFilter(func(name string) bool { return name == "lo0" || name == "lo" })
	minecraft.RegisterNetwork("nethernet", func(logger *slog.Logger) minecraft.Network {
		return minecraft.NetherNet{Dialer: nethernet.Dialer{API: webrtc.NewAPI(webrtc.WithSettingEngine(settings))}, Log: logger}
	})

	flag.IntVar(&groupSize, "group-size", 50, "players per group (even, 2..50)")
	count := flag.Int("players", 4, "connections")
	seconds := flag.Int("seconds", 30, "measurement duration")
	settle := flag.Int("settle-seconds", 15, "settling duration before measurement")
	address := flag.String("address", "http://127.0.0.1:29867", "server")
	out := flag.String("out", "network.json", "result")
	pprof := flag.String("pprof", "", "optional local profiling address")
	flag.Parse()
	if *count < 1 || *count > 1000 || *seconds < 1 || *settle < 0 || groupSize < 2 || groupSize > 50 || groupSize%2 != 0 {
		fmt.Fprintln(os.Stderr, "players must be 1..1000, seconds positive, and group-size even within 2..50")
		os.Exit(2)
	}
	if *pprof != "" {
		go http.ListenAndServe(*pprof, nil)
	}
	bots := make([]*Bot, *count)
	joined := make(chan bool, *count)
	for i := range bots {
		bots[i] = &Bot{stats: Stats{Name: fmt.Sprintf("Net%04d", i)}}
		go run(bots[i], i, *address, joined)
		time.Sleep(50 * time.Millisecond)
	}
	online := 0
	for range bots {
		if <-joined {
			online++
		}
	}
	fmt.Printf("READY %d/%d\n", online, *count)
	time.Sleep(time.Duration(*settle) * time.Second)
	active.Store(true)
	fmt.Println("MEASURE")
	time.Sleep(time.Duration(*seconds) * time.Second)
	active.Store(false)
	fmt.Println("MEASURE_END")
	bufio.NewReader(os.Stdin).ReadString('\n')
	close(quit)
	result := []Stats{}
	for _, b := range bots {
		b.mu.Lock()
		result = append(result, b.stats)
		if b.conn != nil {
			b.conn.Close()
		}
		b.mu.Unlock()
	}
	data, _ := json.MarshalIndent(result, "", "  ")
	os.WriteFile(*out, data, 0644)
	fmt.Println("DONE", *out)
	if online != *count {
		os.Exit(1)
	}
}
