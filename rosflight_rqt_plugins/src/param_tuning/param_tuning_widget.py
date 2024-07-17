import os
import yaml

from ament_index_python import get_resource
from python_qt_binding import loadUi
from python_qt_binding.QtCore import pyqtSlot, QModelIndex
from python_qt_binding.QtGui import QStandardItemModel, QStandardItem
from python_qt_binding.QtWidgets import QWidget, QPushButton, QFileDialog


class ParamTuningWidget(QWidget):
    def __init__(self, config: dict, paramClient, paramFilepath: str, x_length: float):
        self.paramFilePath = paramFilepath

        # Initialize widget
        super(ParamTuningWidget, self).__init__()
        self.setObjectName('ParamTuningWidget')
        self.addChangedValuesToHist = True

        # Load the UI file
        _, path = get_resource('packages', 'rosflight_rqt_plugins')
        uiFile = os.path.join(path, 'share', 'rosflight_rqt_plugins', 'resources', 'param_tuning.ui')
        loadUi(uiFile, self)

        # Define table formatting
        self.config = config
        self.tableHeaders = ['Parameter', 'Value', 'Description', 'Reset to Previous', 'Reset to Initial']
        self.tableWidths = [175, 125, 500, 250, 250]

        # Get the original values of the parameters
        self.paramClient = paramClient
        self.valueStack = {}
        for group in config:
            for param in config[group]['params']:
                value = self.paramClient.get_param(group, param)
                self.valueStack[(group, param)] = [value]

        # Set up the widget
        # Group selection - QComboBox
        self.groupSelection.addItems(self.config.keys())
        self.groupSelection.currentTextChanged.connect(self.groupSelectionCallback)
        # Refresh button - QPushButton
        self.refreshButton.clicked.connect(self.refreshButtonCallback)
        # Save to file button - QPushButton
        self.saveButton.clicked.connect(self.saveButtonCallback)
        # Parameter table - QTableView
        self.setupTableModels()
        self.createTableButtons()
        self.refreshTableValues()

    def setupTableModels(self):
        # Create a model for every group
        self.models = {}
        self.currentGroupKey = list(self.config.keys())[0]
        for group in self.config:
            model = QStandardItemModel()
            model.setHorizontalHeaderLabels(self.tableHeaders)
            for param in self.config[group]['params']:
                desc = self.config[group]['params'][param]['description']
                param_item = QStandardItem(param)
                param_item.setEditable(False)
                value_item = QStandardItem('0.0')
                desc_item = QStandardItem(desc)
                desc_item.setEditable(False)
                model.appendRow([param_item, value_item, desc_item])
            self.models[group] = model

        # Load the first model into the table
        self.paramTableView.setModel(self.models[self.currentGroupKey])

        # Set the column widths
        for i, width in enumerate(self.tableWidths):
            self.paramTableView.setColumnWidth(i, width)

        # Hide the number row
        self.paramTableView.verticalHeader().hide()

        # Connect the model change signal
        self.paramTableView.model().dataChanged.connect(self.onModelChange)

    def createTableButtons(self):
        # Get a list of gains for the current group
        currentGroup = self.config[self.currentGroupKey]
        currentParams = list(currentGroup['params'].keys())

        for i, param in enumerate(currentParams):
            # Create reset to previous buttons
            previousButtonValue = self.valueStack[(self.currentGroupKey, param)][-2] \
                if len(self.valueStack[(self.currentGroupKey, param)]) > 1 \
                else self.valueStack[(self.currentGroupKey, param)][0]

            button = QPushButton(str(previousButtonValue))
            button.clicked.connect(
                lambda _, g=self.currentGroupKey, index=i, p=param: self.resetPreviousButtonCallback(g, index, p)
            )
            index = self.paramTableView.model().index(i, 3)
            self.paramTableView.setIndexWidget(index, button)

            # Create reset to original buttons
            button = QPushButton(str(self.valueStack[(self.currentGroupKey, param)][0]))
            button.clicked.connect(
                lambda _, g=self.currentGroupKey, index=i, p=param: self.resetInitialButtonCallback(g, index, p)
            )
            index = self.paramTableView.model().index(i, 4)
            self.paramTableView.setIndexWidget(index, button)

    def resetPreviousButtonCallback(self, group, row, param):
        # Pop the last value from list, unless it is the last value
        if len(self.valueStack[(group, param)]) > 1:
            self.valueStack[(group, param)].pop()
        value = self.valueStack[(group, param)][-1]

        # Update the table
        self.addChangedValuesToHist = False
        self.models[group].item(row, 1).setText(str(value))
        self.addChangedValuesToHist = True

    def resetInitialButtonCallback(self, group, row, param):
        self.models[group].item(row, 1).setText(str(self.valueStack[(group, param)][0]))
        self.valueStack[(group, param)] = [self.valueStack[(group, param)][0]]
        self.createTableButtons()

    def refreshTableValues(self):
        # Temporarily disconnect the model change signal
        self.paramTableView.model().dataChanged.disconnect(self.onModelChange)

        # Get current values of the parameters
        for i in range(self.models[self.currentGroupKey].rowCount()):
            param = self.models[self.currentGroupKey].item(i, 0).text()
            value = self.paramClient.get_param(self.currentGroupKey, param)

            # If the value is different from the previous value, add it to the stack and update the buttons
            if value != self.valueStack[(self.currentGroupKey, param)][-1]:
                self.valueStack[(self.currentGroupKey, param)].append(value)
                self.createTableButtons()

            # Update the table
            self.models[self.currentGroupKey].item(i, 1).setText(str(value))

        # Reconnect the model change signal
        self.paramTableView.model().dataChanged.connect(self.onModelChange)

    def groupSelectionCallback(self, text):
        self.currentGroupKey = text

        # Swap models and connect the model change signal to the new model
        self.paramTableView.model().dataChanged.disconnect(self.onModelChange)
        self.paramTableView.setModel(self.models[text])
        self.paramTableView.model().dataChanged.connect(self.onModelChange)

        # Update table
        self.createTableButtons()
        self.refreshTableValues()

    def refreshButtonCallback(self):
        self.refreshTableValues()

    def saveButtonCallback(self):
        # Request a filepath is a param filepath hasn't already been given
        if self.paramFilePath is None:
            options = QFileDialog.Options()
            filepath, _ = QFileDialog.getSaveFileName(None, 'Save Parameters to ROS .yaml', '', 'YAML Files (*.yaml)',
                                                      options=options)
            if not filepath:
                self.paramClient.print_warning('No file selected, parameters not saved.')
                return
        else:
            filepath = self.paramFilePath

        # Load the existing parameter file if it exists
        if os.path.exists(filepath):
            with open(filepath, 'r') as file:
                params = yaml.safe_load(file)
        else:
            params = {}

        # Create a dictionary formatted for ROS parameters, based on the current ROS parameters
        for group in self.config:
            param_dict = {}
            for i in range(self.models[group].rowCount()):
                param_name = self.models[group].item(i, 0).text()
                param_dict[param_name] = self.paramClient.get_param(group, param_name, False)

            # Add new items to dictionary, appending it if already exists
            stripped_node_name = self.config[group]['node'].lstrip('/')
            if stripped_node_name in params:
                params[stripped_node_name]['ros__parameters'].update(param_dict)
            else:
                params[stripped_node_name] = {'ros__parameters': param_dict}

        # Save the dictionary to the file
        with open(filepath, 'w') as file:
            yaml.dump(params, file)

    @pyqtSlot(QModelIndex, QModelIndex)
    def onModelChange(self, topLeft, bottomRight):
        # Check if the value is a number
        try:
            value = float(topLeft.data())
        except ValueError:
            self.paramClient.print_warning('Invalid value type, please enter a number.')
            self.refreshTableValues()
            return

        # Set the new value
        param = self.models[self.currentGroupKey].item(topLeft.row(), 0).text()
        self.paramClient.set_param(self.currentGroupKey, param, value)

        # Add the new value to the previous values list
        if self.addChangedValuesToHist:
            self.valueStack[(self.currentGroupKey, param)].append(value)

        # Update the buttons with the new previous value
        # Creating all new buttons is inefficient, but it is the easiest and most consistent way to update the values
        self.createTableButtons()
