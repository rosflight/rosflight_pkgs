import argparse
import yaml

import rclpy
from rqt_gui_py.plugin import Plugin
from python_qt_binding.QtWidgets import QFileDialog, QVBoxLayout

from .param_tuning_client import ParameterClient
from .param_tuning_plotter import ParamTuningPlotter
from .param_tuning_widget import ParamTuningWidget


class ParamTuning(Plugin):

    def __init__(self, context):
        super(ParamTuning, self).__init__(context)
        self.setObjectName('ParamTuning')

        self._context = context
        self._node = context.node

        # Load the configuration file
        args = self._parse_args(context.argv())
        if args.config_filepath is None:
            options = QFileDialog.Options()
            filename, _ = QFileDialog.getOpenFileName(None, 'Open Configuration File', '', 'YAML Files (*.yaml)',
                                                      options=options)
            if not filename:
                self._node.get_logger().fatal('No configuration file selected, please provide a configuration file like'
                                              ' rosflight_rqt_plugins/resources/example_config.yaml')
                raise RuntimeError('No configuration file provided')

            args.config_filepath = filename
        with open(args.config_filepath, 'r') as file:
            self._config = yaml.safe_load(file)

        # Get the filepath to save params to
        self._param_filepath = args.param_filepath

        # Initialize the ROS client
        self._client = ParameterClient(self._config, self._node)

        # Initialize the widget
        self._widget = ParamTuningWidget(self._config, self._client, self._param_filepath)
        if context.serial_number() > 1:
            self._widget.setWindowTitle(
                self._widget.windowTitle() + (' (%d)' % context.serial_number()))

        # Initialize the plotter
        plot_layout = self._widget.findChild(QVBoxLayout, 'plot_layout')
        self._plotter = ParamTuningPlotter(self._config, self._client, plot_layout)
        self._widget.register_plot_swap_callback(self._plotter.switch_plot_group)
        self._widget.register_duration_change_callback(self._client.set_data_hist_duration)
        def pause_plotting(pause: bool):
            self._plotter.pause_plotting(pause)
            self._client.pause_data_collection(pause)
        self._widget.register_pause_plot_callback(pause_plotting)

        context.add_widget(self._widget)

    def _parse_args(self, argv):
        parser = argparse.ArgumentParser(prog='param_tuning', add_help=False)
        ParamTuning.add_arguments(parser)
        return parser.parse_args(argv)

    @staticmethod
    def add_arguments(parser):
        group = parser.add_argument_group('Options for param_tuning plugin')
        group.add_argument('--config-filepath', type=str, help='Path to the .yaml GUI configuration file')
        group.add_argument('--param-filepath', type=str, help='Path to the ROS .yaml parameter file, to save the'
                                                              ' parameters to')

    def shutdown_plugin(self):
        self._client.shutdown()
        self._plotter.shutdown()
        self._node.destroy_node()
        rclpy.shutdown()
