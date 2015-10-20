
package conf

type Config struct {
    Spider struct {
        UrlListFile string
        OutputDirectory bool
        MaxDepth int
        CrawlInterval int
        CrawlTimeout int
        TargetUrl string
        ThreadCount int
    }
}
