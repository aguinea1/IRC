################################################################################
#                              CONFIGURATION                                   #
################################################################################

NAME			= ircserv
CC			= c++
CFLAGS 			= -Wall -Wextra -Werror -Wno-shadow -std=c++98 -MMD -fsanitize=address
DEPFLAGS		= -MMD -MF $(DEPDIR)/$*.d

SRCDIR			= srcs/
OBJDIR			= obj
DEPDIR			= deps

################################################################################
#                              SOURCE FILES                                    #
################################################################################

SRCS =	$(SRCDIR)/main.cpp		\
		$(SRCDIR)/Server.cpp	\
		$(SRCDIR)/Client.cpp

HEADERS =	$(SRCDIR)/Server.hpp		\
			$(SRCDIR)/Client.hpp		


################################################################################
#                             OBJECTS & DEPS                                   #
################################################################################

OBJS			= $(SRCS:%.cpp=$(OBJDIR)/%.o)
DEPS			= $(OBJS:%.o=$(DEPDIR)/%.d)

################################################################################
#                              RULES                                           #
################################################################################

all: dir $(NAME)

$(NAME): Makefile $(OBJS)
	@$(CC) $(CFLAGS) $(OBJS) -o $@
	@echo "\033[1;32m ➜ ➜ I R C  [✔]\033[0m"


$(OBJDIR)/%.o: %.cpp $(HEADERS)
	@mkdir -p $(dir $@) $(dir $(DEPDIR)/$*.d)
	@$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDE) -c $< -o $@

dir:
	@mkdir -p $(OBJDIR) $(DEPDIR)

clean:
	@rm -rf $(OBJDIR) $(DEPDIR)
	@echo "\033[1;32m ♻ CHAUUU ♻\033[0m"

fclean: clean
	@rm -f $(NAME)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re dir
