package apm_module

import (
	"fmt"
	"github.com/go-martini/martini"
	metrics "github.com/yvasiyarov/go-metrics"
	"baidu/gobaidu"
	"time"
)

var agent *gobaidu.Agent

func Handler(c martini.Context) {
	startTime := time.Now()
	c.Next()
	agent.HTTPTimer.UpdateSince(startTime)
}

func InitNewrelicAgent(license string, appname string, verbose bool) error {

	if license == "" {
		return fmt.Errorf("Please specify baidu license")
	}

	agent = gobaidu.NewAgent()
	agent.NewrelicLicense = license

	agent.HTTPTimer = metrics.NewTimer()
	agent.CollectHTTPStat = true
	agent.Verbose = verbose

	agent.NewrelicName = appname
	agent.Run()
	return nil
}
