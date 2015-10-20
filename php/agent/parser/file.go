/*
 * file.go
 *
 *  Created on: 13/08/2015
 *      Author: miaohong(miaohong01@baidu.com)
 */

package parser

import (
 	"os"
 	"io/ioutil"
 	"../glog"
)

type DataFile struct {
	filePath    string
	content     []byte
}

func NewDataFile(filePath string) *DataFile {
	return &DataFile {
		filePath : filePath,
	}
}

func (f *DataFile) isFileExist() bool {
	_, err := os.Stat(f.filePath)
	if err != nil && os.IsNotExist(err) {
		return false
	}

	return true
}

func (f *DataFile) rmFile() error {
	err := os.Remove(f.filePath)
    if err != nil {
    	glog.Error(err.Error())
		return err
    } 

    return nil
}

// func cat(scanner *bufio.Scanner) error {
//     for scanner.Scan() {
//         fmt.Println(scanner.Text()) 
    
//     }
//     return scanner.Err()
// }

func (f *DataFile) GetAllContent() error {
	var err error
	exist := f.isFileExist()
	if !exist {
		glog.Error(FILE_NOT_EXIST)
		return FILE_NOT_EXIST
	}

	f.content, err = ioutil.ReadFile(f.filePath)
    if err != nil {
    	glog.Error(err.Error())
		return err
    } 

    return err
}