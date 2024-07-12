import rclpy
from rclpy.node import Node
from rclpy.parameter import ParameterType, Parameter
from rcl_interfaces.srv import SetParameters, GetParameters
from rosidl_runtime_py.utilities import get_message


class ParameterClient():
    def __init__(self, config: dict, node: Node):
        self.config = config
        self.node = node

        # Initialize parameter clients
        self.set_clients = {}
        self.get_clients = {}
        for group in config:
            node_name = config[group]['node']
            if node_name not in self.set_clients:
                self.set_clients[node_name] = self.node.create_client(SetParameters, f'{node_name}/set_parameters')
                while not self.set_clients[node_name].wait_for_service(timeout_sec=1.0):
                    self.node.get_logger().info(f'{node_name}/set_parameters service not available, waiting...')
                self.get_clients[node_name] = self.node.create_client(GetParameters, f'{node_name}/get_parameters')
                while not self.get_clients[node_name].wait_for_service(timeout_sec=1.0):
                    self.node.get_logger().info(f'{node_name}/get_parameters service not available, waiting...')

        # Initialize topics subscribers for plotting
        self.plot_subscribers = {}
        for group in config:
            if 'plot_topics' in config[group]:
                for topic in config[group]['plot_topics']:
                    topic_name = config[group]['plot_topics'][topic].split('/')[1]
                    field_name = config[group]['plot_topics'][topic].split('/')[2]
                    if topic_name not in self.plot_subscribers:
                        message_type = self.get_message_type(topic_name)
                        if message_type is None:
                            self.node.get_logger().error(f'Failed to get message type for {topic_name},'
                                                    f' does the topic exist?')
                        else:
                            self.plot_subscribers[topic_name] = self.node.create_subscription(
                                message_type,
                                topic_name,
                                lambda msg, f=field_name: self.message_callback(msg, f),
                                10)

    def get_message_type(self, topic_name: str):
        topic_array = self.node.get_topic_names_and_types()
        for topic in topic_array:
            if topic[0] == '/' + topic_name:
                return get_message(topic[1][0])
        return None

    def message_callback(self, msg, field_name: str):
        value = getattr(msg, field_name)
        print(value)

    def get_param(self, group: str, param: str, scaled: bool = True) -> float:
        if not scaled or 'scale' not in self.config[group]['params'][param]:
            scale = 1.0
        else:
            scale = self.config[group]['params'][param]['scale']
        node_name = self.config[group]['node']

        request = GetParameters.Request()
        request.names = [param]

        future = self.get_clients[node_name].call_async(request)
        rclpy.spin_until_future_complete(self.node, future)
        if future.result() is not None:
            if len(future.result().values) == 0:
                self.node.get_logger().error(f'Parameter {param} not found')
                return 0.0
            if future.result().values[0].type == ParameterType.PARAMETER_DOUBLE:
                return future.result().values[0].double_value * scale
            else:
                self.node.get_logger().error(f'Unsupported parameter type for {param}, only double is supported')
        else:
            self.node.get_logger().error('Service call failed %r' % (future.exception(),))

    def set_param(self, group: str, param: str, value: float, scaled: bool = True) -> None:
        if scaled and 'scale' in self.config[group]['params'][param]:
            value /= self.config[group]['params'][param]['scale']

        node_name = self.config[group]['node']
        request = SetParameters.Request()
        parameter = Parameter(param, Parameter.Type.DOUBLE, value)
        request.parameters.append(parameter.to_parameter_msg())
        future = self.set_clients[node_name].call_async(request)
        rclpy.spin_until_future_complete(self.node, future)
        if future.result() is not None:
            if future.result().results[0].successful:
                self.node.get_logger().info(f'Set {node_name}/{param} to {value}')
            else:
                self.node.get_logger().error(f'Failed to set {param} to {value}: {future.result().results[0].reason}')
        else:
            self.node.get_logger().error('Service call failed %r' % (future.exception(),))

    def print_info(self, message: str):
        self.node.get_logger().info(message)

    def print_warning(self, message: str):
        self.node.get_logger().warning(message)

    def print_error(self, message: str):
        self.node.get_logger().error(message)

    def print_fatal(self, message: str):
        self.node.get_logger().fatal(message)
