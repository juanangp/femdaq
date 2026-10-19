## `femdaq`

New version of DAQ code for AFTER/AGET electronics, it supports DCC, FEMINOS and ARC electronics, based on `mclient` program from Denis Calvet.

# Code features

* Console based on readline, which include history of the commands.
* Using yaml format for config files
* Data is stored using ROOT, data is stored without using dictionaries for progam compatibility,
  metadata (configuration file, start and end time) is stored as TObjString.
* Maximum file size configurable from config file.
* Multithreading has been added for receiving commands and event builder.
* Exit handler, closing the program properly after pressing `CTRL-C`.
* Added a log file per FEM to check pedestal equalization and other command replies.
* Online file visualization using ROOT GUI.

### Pre-requisites
* Readline (libreadline-dev)
* yaml-cpp (libyaml-cpp-dev)
* ROOT (version > 6.24) in C++17

### Installation

To build the project, run the following commands from the repository:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/your/custom/install/path
cmake --build build
```

Afterwards you have to source the code.

```bash
source /your/custom/install/path/thisFEMDAQ.sh
```

The programs uses `CMake` `FetchContent` module to download the required CL11 dependencies.

### Usage

#### femclient

Similar to `mclient` commands, but it requires a YAML configuration file for initialization.
To get a list of the available options, run the following command:

```bash
./femclient --help
```

The `femclient` program requires a configuration file for some DAQ parameters:

```bash
./femclient -c myConfigFile.yml
```

Standard usage is similar as the original `mclient`, commands are sent and received from the DAQ computer to the electronics
using a command fetcher. However there are new `femclient` commands that has to be issued for standard operation:

* `addMetadata` update metadata information in the configuration file, this metadata is updated in the yaml file and stored in
the root file.

* `fopen` to open ROOT and/or log files, two arguments are available `root` for only generate ROOT files and `log` to only
generate log files (e.g. pedestals), if no arguments are added it generate both logs and ROOT files. Note that you have to
issue addMetadata before fopen if you want to store data taking info in the ROOT file.

* `fclose` to close ROOT and/or log files.

* `startDAQ` starts acquisition loop, it replaces the acquisition loop in original `mclient` program, note that the maximum
run time and number of events is now defined in the configuration file.

* `stopDAQ` stop event builder and clean-up after stopping the run.

* `Pedestals` only avalilable for DCC electronics to start a pedestal loop.

* `verbose` set verbose level, three options are available `silent`, `info` or `debug`, note that it overrides verbosity defined
in the configuration file.

Note that some legacy commands form former `mclient` program are not supported (e.g. `DAQ`, `credits restore`,...), although this
shouldn't be an issue, some error messages will be prompted.

### Configuration file

Configuration file example is shown below:

```yaml
run:
  rawDataPath: /my/data/storage/area/
  experiment: Test
  tag: DUMMY
  type: background
  verbose: info
  nEvents: 0
  time: 0s
  maxFileSize: 100Mb
  updateRate: 5s
  electronics: ARC
  daqCredits: 5440
  daqCreditThreshold: 272
  resync: true
  FEM:
    - id: 0
      IP: 192.168.10.2
  Info:
    MeshV: 300
    DriftV: 500
    PressureB: 1.4
```

Note that the fields under the node `Info` are not required, other fields can be added under this node and they will be
requested to be updated after issue `addMetadata`

The following optional fields apply to `ARC` and `FEMINOS` electronics:

* `daqCredits`: number of data frames a FEM may send before it receives new credits. The credits of the frames
received are requested again as soon as `daqCreditThreshold` frames are pending, so there are never more than
`daqCredits` frames in flight. A small value avoids that the switch or the socket buffers overflow when several FEMs
send at the same time, which loses UDP frames. Defaults: 8 frames and threshold 1 for `FEMINOS` (as
`credits restore 8 1 F` in `mclient`), 5440 frames and threshold 272 for `ARC`.
* `resync`: if `true` (default) the events of the FEMs are matched by timestamp: when a FEM misses an event (e.g. lost
frames) that event is built without it, instead of shifting all the next events of the run. It requires the
timestamps of the FEMs to be synchronized (TCM timestamp clear). If `false` the events are matched by arrival order.

For `FEMINOS` the sequence number of the data frames is checked: lost frames are reported in the console and the FEM
logs (`[SYNC]` messages), their credits are restored and the incomplete events are dropped. The `femID`,
`femEventCount` and `femTimestamp` branches of the ROOT file list the FEMs included in each event.

### Event display

A macro using ROOT GUI is provided for event visualization. To run the EventDisplay issue, the following command:

`FEMDAQRoot -l -e "EventDisplay()"`

Note that it uses `FEMDaqRoot` which starts a ROOT application with pre-loaded femdaq macros. A GUI will prompt:

![image](https://github.com/user-attachments/assets/36634fbc-d033-4975-b039-5a9b472c4c59)
