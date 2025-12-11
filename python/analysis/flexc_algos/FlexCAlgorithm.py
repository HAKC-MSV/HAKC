import abc

from hakc.HAKCDatabase import HAKCDatabase


class FlexCAlgorithm(abc.ABC):

    def __init__(self, parser, **kwargs):
        algo_parser = parser.add_parser(self.title, aliases=self.aliases, help=self.help)
        self.add_command_line_arguments(algo_parser)

    @property
    @abc.abstractmethod
    def title(self) -> str:
        raise NotImplementedError

    @property
    def aliases(self) -> list[str]:
        return []

    @property
    @abc.abstractmethod
    def help(self) -> str:
        raise NotImplementedError

    def add_command_line_arguments(self, parser):
        pass

    @abc.abstractmethod
    def run(self, db: HAKCDatabase, **kwargs):
        raise NotImplementedError

    def __str__(self):
        return f'Algorithm {self.title}'

    def __eq__(self, other):
        if isinstance(other, str):
            return other in self.aliases or other == self.title
        elif isinstance(other, FlexCAlgorithm):
            return other.title == self.title
        return False
